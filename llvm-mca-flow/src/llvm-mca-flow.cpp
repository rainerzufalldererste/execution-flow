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

#include "llvm-mca-flow.h"

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
#include "llvm/MCA/CustomBehaviour.h"
#include "llvm/MCA/HWEventListener.h"
#include "llvm/MCA/InstrBuilder.h"
#include "llvm/MCA/Pipeline.h"
#include "llvm/MCA/SourceMgr.h"
#include "llvm/MCA/Stages/Stage.h"
#include "llvm/MCA/Stages/InstructionTables.h"
#pragma warning (pop)

////////////////////////////////////////////////////////////////////////////////

class FetchStage final : public llvm::mca::Stage
{
private:
  llvm::mca::InstRef lastInstruction;
  llvm::mca::SourceMgr *pSource; // initialized in the constructor.
  std::queue<std::unique_ptr<llvm::mca::Instruction>> referencedInstructions;

  llvm::Error iterateSource();

public:
  inline FetchStage(llvm::mca::SourceMgr *pSource) : pSource(pSource) {}

  bool isAvailable(const llvm::mca::InstRef &IR) const override;
  bool hasWorkToComplete() const override;
  llvm::Error execute(llvm::mca::InstRef &IR) override;
  llvm::Error cycleStart() override;
  llvm::Error cycleResume() override;
  llvm::Error cycleEnd() override;
};

////////////////////////////////////////////////////////////////////////////////

class FlowView final : public llvm::mca::HWEventListener
{
private:
  PortUsageFlow *pFlow; // initialized in the constructor.
  size_t instructionIndex = 0;

public:
  inline FlowView(PortUsageFlow *pFlow) : pFlow(pFlow) {}

  inline void onCycleEnd() override { instructionIndex++; }

  void onEvent(const llvm::mca::HWInstructionEvent &evnt) override;
  void onEvent(const llvm::mca::HWStallEvent &evnt) override;
  void onEvent(const llvm::mca::HWPressureEvent &evnt) override;
};

////////////////////////////////////////////////////////////////////////////////

#pragma optimize ("", off)

bool llvm_mca_flow_create(const void *pAssembledBytes, const size_t assembledBytesLength, PortUsageFlow *pFlow)
{
  if (pFlow == nullptr)
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
  std::unique_ptr<const llvm::Target> target(llvm::TargetRegistry::lookupTarget(targetTriple.str(), errorString));

  // Did we get one? (I sure hope so!)
  if (target == nullptr)
    return false;

  // Create everything the context wants.
  llvm::MCTargetOptions targetOptions(llvm::mc::InitMCTargetOptionsFromFlags());
  std::unique_ptr<llvm::MCRegisterInfo> registerInfo(target->createMCRegInfo(targetTriple.str()));
  std::unique_ptr<llvm::MCAsmInfo> asmInfo(target->createMCAsmInfo(*registerInfo, targetTriple.str(), targetOptions));
  std::unique_ptr<llvm::MCSubtargetInfo> subtargetInfo(target->createMCSubtargetInfo(targetTriple.str(), llvm::sys::getHostCPUName(), ""));

  // Create Machine Code Context from the triple.
  llvm::MCContext context(targetTriple, asmInfo.get(), registerInfo.get(), subtargetInfo.get());

  // Get the disassembler.
  std::unique_ptr<llvm::MCDisassembler> disasm(target->createMCDisassembler(*subtargetInfo, context));

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
        flow.perClockInstruction.emplace_back(decodedInstructions.size(), i);
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
  std::unique_ptr<llvm::MCInstrInfo> instructionInfo(target->createMCInstrInfo());
  std::unique_ptr<llvm::MCInstrAnalysis> instructionAnalysis(target->createMCInstrAnalysis(instructionInfo.get()));
  llvm::mca::InstrPostProcess postProcess(*subtargetInfo, *instructionInfo);
  std::unique_ptr<llvm::mca::InstrumentManager> instrumentManager(target->createInstrumentManager(*subtargetInfo, *instructionInfo));

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
  llvm::mca::CircularSourceMgr source(mcaInstructions, 2);

  // Create and fill the pipeline with the source.
  std::unique_ptr<llvm::mca::Pipeline> pipeline = std::make_unique<llvm::mca::Pipeline>();
  pipeline->appendStage(std::make_unique<FetchStage>(&source));

  const llvm::MCSchedModel &schedulerModel = subtargetInfo->getSchedModel();
  pipeline->appendStage(std::make_unique<llvm::mca::InstructionTables>(schedulerModel));

  // Get Stages from Scheduler model.
  {
    const size_t resourceTypeCount = schedulerModel.getNumProcResourceKinds();
    size_t validTypeIndex = (size_t)-1;

    for (uint32_t i = 1; i < resourceTypeCount; i++) // index 0 appears to be used as `null`-index.
    {
      const llvm::MCProcResourceDesc *pResource = schedulerModel.getProcResource(i);
      const size_t perResourcePortCount = pResource->NumUnits;

      if (perResourcePortCount == 0)
        continue;

      ++validTypeIndex;

      for (uint32_t j = 0; j < perResourcePortCount; j++)
      {
        std::string name = pResource->Name;

        if (perResourcePortCount > 1)
          name = name + " " + std::to_string(j + 1);

        flow.ports.emplace_back(validTypeIndex, j, name);
      }
    }
  }

  // Create event handler to observe simulated hardware events.
  FlowView flowView(&flow);
  pipeline->addEventListener(&flowView);

  // Run the pipeline.
  pipeline->run();

  *pFlow = flow;

  return result;
}

////////////////////////////////////////////////////////////////////////////////

llvm::Error FetchStage::iterateSource()
{
  assert(!lastInstruction && "The last instruction should've been invalidated by now");

  if (!pSource->hasNext())
  {
    if (pSource->isEnd())
      return llvm::make_error<llvm::mca::InstStreamPause>();

    return llvm::ErrorSuccess();
  }

  // Retrieve next instruction.
  llvm::mca::SourceRef nextInstruction = pSource->peekNext();

  // Add to referenced instructions and make it the active instruction.
  std::unique_ptr<llvm::mca::Instruction> referencableInstruction = std::make_unique<llvm::mca::Instruction>(nextInstruction.second);
  lastInstruction = llvm::mca::InstRef(nextInstruction.first, referencableInstruction.get());
  referencedInstructions.push(std::move(referencableInstruction));
  
  // Move the source forward.
  pSource->updateNext();
  
  return llvm::ErrorSuccess();
}

bool FetchStage::isAvailable(const llvm::mca::InstRef &instruction) const
{
  (void)instruction; // we don't need this, as we're merely imitating the instruction fetching procedure.

  if (lastInstruction)
    return checkNextStage(lastInstruction);

  return false;
}

bool FetchStage::hasWorkToComplete() const
{
  if (lastInstruction)
    return true;

  return !pSource->isEnd();
}

llvm::Error FetchStage::execute(llvm::mca::InstRef &instruction)
{
  (void)instruction; // we don't need this, as we're merely imitating the instruction fetching procedure.

  assert(!lastInstruction && "No Instruction active.");

  llvm::Error result = moveToTheNextStage(instruction);

  if (!result.success())
    return std::move(result);

  instruction.invalidate();

  return iterateSource();
}

llvm::Error FetchStage::cycleStart()
{
  if (!lastInstruction)
    return iterateSource();
  else
    return llvm::ErrorSuccess();
}

llvm::Error FetchStage::cycleResume()
{
  assert(!lastInstruction && "No Instruction active.");

  return iterateSource();
}

llvm::Error FetchStage::cycleEnd()
{
  // Remove all instructions that aren't referenced anymore.
  while (referencedInstructions.size())
  {
    const auto &first = referencedInstructions.front();

    if (!first->isRetired())
      break;

    referencedInstructions.pop();
  }

  return llvm::ErrorSuccess();
}

////////////////////////////////////////////////////////////////////////////////

void FlowView::onEvent(const llvm::mca::HWInstructionEvent &evnt)
{
  (void)evnt;
}

void FlowView::onEvent(const llvm::mca::HWStallEvent &evnt)
{
  (void)evnt;
}

void FlowView::onEvent(const llvm::mca::HWPressureEvent &evnt)
{
  (void)evnt;
}
