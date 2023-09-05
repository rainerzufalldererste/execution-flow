////////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2023, Christoph Stiller. All rights reserved.
// 
// Redistribution and use in next and binary forms, with or without 
// modification, are permitted provided that the following conditions are met:
// 
// 1. Redistributions of next code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation 
//    and/or other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
////////////////////////////////////////////////////////////////////////////////

#include "execution-flow.h"

#include "FlowView.h"

#include <algorithm>
#include <queue>

#pragma warning (push, 0)
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/TargetParser/SubtargetFeature.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCInstBuilder.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCTargetOptions.h"
#include "llvm/MC/MCTargetOptionsCommandFlags.h"
#include "llvm/MC/MCDisassembler/MCDisassembler.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/MCA/Context.h"
#include "llvm/MCA/CustomBehaviour.h"
#include "llvm/MCA/InstrBuilder.h"
#include "llvm/MCA/Pipeline.h"
#include "llvm/MCA/SourceMgr.h"
#include "llvm/MCA/Stages/Stage.h"
#include "llvm/MCA/Stages/InstructionTables.h"
#pragma warning (pop)

////////////////////////////////////////////////////////////////////////////////

static const char *CoreArchitectureLookup[] =
{
  nullptr,
  "alderlake",
  "broadwell",
  "cannonlake",
  "cascadelake",
  "cooperlake",
  "emeraldrapids",
  "goldmont",
  "goldmont_plus",
  "grandridge",
  "graniterapids",
  "haswell",
  "icelake_client",
  "icelake_server",
  "ivybridge",
  "meteorlake",
  "raptorlake",
  "rocketlake",
  "sandybridge",
  "sapphirerapids",
  "sierraforest",
  "silvermont",
  "skylake",
  "skx",
  "skylake_avx512",
  "tigerlake",
  "tremont",
  "znver1",
  "znver2",
  "znver3",
  "znver4",
};

static_assert(std::size(CoreArchitectureLookup) == (size_t)CoreArchitecture::_Count);

const char *core_arch_to_string(const CoreArchitecture arch);

////////////////////////////////////////////////////////////////////////////////

bool execution_flow_create(const void *pAssembledBytes, const size_t assembledBytesLength, PortUsageFlow *pFlow, const CoreArchitecture arch, const size_t iterations, const size_t relevantIteration)
{
  if (pFlow == nullptr || pAssembledBytes == nullptr || (uint64_t)arch > (uint64_t)CoreArchitecture::_Count || relevantIteration >= iterations || iterations > UINT32_MAX)
    return false;

  static llvm::mc::RegisterMCTargetOptionsFlags targetOptionFlags;

  LLVMInitializeX86TargetInfo();
  LLVMInitializeX86TargetMC();
  LLVMInitializeX86Target();
  LLVMInitializeX86Disassembler();

  // Get Target Triple from the current host.
  const std::string targetTripleName = llvm::Triple::normalize(llvm::sys::getDefaultTargetTriple());
  const llvm::Triple targetTriple(targetTripleName);

  // Look up the target triple to get a hold of the target.
  std::string errorString;
  const llvm::Target *pTarget(llvm::TargetRegistry::lookupTarget(targetTriple.str(), errorString));

  // Did we get one? (I sure hope so!)
  if (pTarget == nullptr)
    return false;

  // Create everything the context wants.
  llvm::MCTargetOptions targetOptions(llvm::mc::InitMCTargetOptionsFromFlags());
  std::unique_ptr<llvm::MCRegisterInfo> registerInfo(pTarget->createMCRegInfo(targetTriple.str()));
  std::unique_ptr<llvm::MCAsmInfo> asmInfo(pTarget->createMCAsmInfo(*registerInfo, targetTriple.str(), targetOptions));
  std::unique_ptr<llvm::MCSubtargetInfo> subtargetInfo;
  
  if (arch == CoreArchitecture::_CurrentCPU)
    subtargetInfo = std::unique_ptr<llvm::MCSubtargetInfo>(pTarget->createMCSubtargetInfo(targetTriple.str(), llvm::sys::getHostCPUName(), ""));
  else
    subtargetInfo = std::unique_ptr<llvm::MCSubtargetInfo>(pTarget->createMCSubtargetInfo(targetTriple.str(), core_arch_to_string(arch), ""));

  // Create Machine Code Context from the triple.
  llvm::MCContext context(targetTriple, asmInfo.get(), registerInfo.get(), subtargetInfo.get());

  // Get the disassembler.
  std::unique_ptr<llvm::MCDisassembler> disasm(pTarget->createMCDisassembler(*subtargetInfo, context));

  // Construct `ArrayRef` to feed the disassembler with.
  llvm::ArrayRef<uint8_t> bytes(reinterpret_cast<const uint8_t *>(pAssembledBytes), assembledBytesLength);

  bool result = true;

  std::vector<llvm::MCInst> decodedInstructions;
  PortUsageFlow flow;

  // Disassemble bytes.
  {
    for (size_t i = 0; i < assembledBytesLength;)
    {
      size_t instructionSize = 1;

      llvm::MCInst retrievedInstruction;
      const llvm::MCDisassembler::DecodeStatus status = disasm->getInstruction(retrievedInstruction, instructionSize, bytes.slice(i), i, llvm::nulls());

      switch (status)
      {
      case llvm::MCDisassembler::Fail: // let's try to squeeze out as many instructions as we can find...
        instructionSize = std::max(instructionSize, (size_t)1);
        result = false;
        break;

      default: // we ignore soft-fails.
        flow.instructionExecutionInfo.emplace_back(decodedInstructions.size(), i);
        decodedInstructions.push_back(retrievedInstruction);
        break;
      }

      i += instructionSize;
    }

    // Have we found something?
    if (decodedInstructions.size() == 0)
      return false;
  }

  // Prepare everything for the instruction builder in order to retrieve `llvm::mca::Instruction`s from the `llvm::Inst`s.
  std::unique_ptr<llvm::MCInstrInfo> instructionInfo(pTarget->createMCInstrInfo());
  std::unique_ptr<llvm::MCInstrAnalysis> instructionAnalysis(pTarget->createMCInstrAnalysis(instructionInfo.get()));
  llvm::mca::InstrPostProcess postProcess(*subtargetInfo, *instructionInfo);
  std::unique_ptr<llvm::mca::InstrumentManager> instrumentManager(pTarget->createInstrumentManager(*subtargetInfo, *instructionInfo));

  if (instrumentManager == nullptr)
    instrumentManager = std::make_unique<llvm::mca::InstrumentManager>(*subtargetInfo, *instructionInfo);

  llvm::mca::InstrPostProcess instructionPostProcess(*subtargetInfo, *instructionInfo);
  llvm::mca::InstrBuilder instructionBuilder(*subtargetInfo, *instructionInfo, *registerInfo, instructionAnalysis.get(), *instrumentManager);
  
  instructionPostProcess.resetState();

  llvm::SmallVector<std::unique_ptr<llvm::mca::Instruction>> mcaInstructions;

  // Retrieve `llvm::mca::Instruction`s.
  for (const auto &instr : decodedInstructions)
  {
    llvm::Expected<std::unique_ptr<llvm::mca::Instruction>> mcaInstr = instructionBuilder.createInstruction(instr, llvm::SmallVector<llvm::mca::Instrument *>()); // from debugging llvm-mca it appears that the second parameter (vector) can be empty (at least whenever there aren't any jumps / calls in the active region.

    if (!mcaInstr)
    {
      result = false;
      break;
    }

    postProcess.postProcessInstruction(mcaInstr.get(), instr);
    mcaInstructions.emplace_back(std::move(mcaInstr.get()));
  }

  // Create source for the `Pipeline` & `HWEventListener`.
  llvm::mca::CircularSourceMgr source(mcaInstructions, (uint32_t)iterations);

  // Create custom behaviour.
  std::unique_ptr<llvm::mca::CustomBehaviour> customBehaviour(pTarget->createCustomBehaviour(*subtargetInfo, source, *instructionInfo));

  if (customBehaviour == nullptr)
    customBehaviour = std::make_unique<llvm::mca::CustomBehaviour>(*subtargetInfo, source, *instructionInfo);

  // Create MCA context.
  llvm::mca::Context mcaContext(*registerInfo, *subtargetInfo);

  const llvm::MCSchedModel &schedulerModel = subtargetInfo->getSchedModel();
  llvm::mca::PipelineOptions pipelineOptions(0, 0, 0, 0, 0, 0, true, true); // this seems very wrong, but that's what llvm-mca is doing and I don't see a way of retrieving the information from the `subtargetInfo` or `schedulerModel`.

  // Create and fill the pipeline with the source.
  std::unique_ptr<llvm::mca::Pipeline> pipeline(mcaContext.createDefaultPipeline(pipelineOptions, source, *customBehaviour));

  // Create instruction printer for the FlowView.
  std::unique_ptr<llvm::MCInstPrinter> instructionPrinter(pTarget->createMCInstPrinter(targetTriple, 1, *asmInfo, *instructionInfo, *registerInfo));

  // Create event handler to observe simulated hardware events.
  FlowView flowView(&flow, schedulerModel, *instructionPrinter, relevantIteration);
  pipeline->addEventListener(&flowView);

  // Get Stages from Scheduler model.
  {
    const size_t resourceTypeCount = schedulerModel.getNumProcResourceKinds();
    size_t validTypeIndex = (size_t)-1;

    for (size_t i = 1; i < resourceTypeCount; i++) // index 0 appears to be used as `null`-index.
    {
      const llvm::MCProcResourceDesc *pResource = schedulerModel.getProcResource((uint32_t)i);
      const size_t perResourcePortCount = pResource->NumUnits;

      if (perResourcePortCount == 0 || pResource->SubUnitsIdxBegin != nullptr) // if `SubUnitsIdxBegin` isn't `nullptr`, there'll be another resource that doesn't indicate *all* of the resources, but the sub-resources individually.
        continue;

      ++validTypeIndex;

      for (size_t j = 0; j < perResourcePortCount; j++)
      {
        std::string name = pResource->Name;

        if (perResourcePortCount > 1)
          name = name + " " + std::to_string(j + 1);

        flowView.addLLVMResourceToPortIndexLookup({ {i, (uint32_t)1 << j }, { flow.ports.size() }});
        flow.ports.emplace_back(validTypeIndex, j, name);
      }
    }
  }

  // Get Register Types and Counts from scheduler extra info.
  if (schedulerModel.hasExtraProcessorInfo())
  {
    const llvm::MCExtraProcessorInfo &extraInfo = schedulerModel.getExtraProcessorInfo();
    
    for (size_t i = 0; i < extraInfo.NumRegisterFiles; i++)
    {
      const llvm::MCRegisterFileDesc &registerFile = extraInfo.RegisterFiles[i];
      const bool registerFileRelevant = (registerFile.NumPhysRegs != 0);

      // Let the flow view know if we're using that register file.
      flowView.addRegisterFileRelevancy(registerFileRelevant);

      if (!registerFileRelevant)
        continue;

      flow.hardwareRegisters.emplace_back(registerFile.Name, registerFile.NumPhysRegs);
    }
  }

  // Run the pipeline.
  llvm::Expected<uint32_t> cycles = pipeline->run();

  if (!cycles)
    result = false;

  *pFlow = std::move(flow);

  return result;
}

const char *core_arch_to_string(const CoreArchitecture arch)
{
  if ((size_t)arch >= std::size(CoreArchitectureLookup))
    return nullptr;

  return CoreArchitectureLookup[(size_t)arch];
}
