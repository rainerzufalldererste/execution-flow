////////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2023, Christoph Stiller. All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice,
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

#pragma warning (push, 0)
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCTargetOptions.h"
#include "llvm/MC/MCTargetOptionsCommandFlags.h"
#include "llvm/MC/MCDisassembler/MCDisassembler.h"
#include "llvm/MC/TargetRegistry.h"
#pragma warning (pop)

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////

bool llvm_mca_flow_create(const void *pAssembledBytes, const size_t assembledBytesLength, PortUsageFlow *pFlow)
{
  if (pFlow == nullptr)
    return false;

  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllDisassemblers();

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
  std::unique_ptr<llvm::MCSubtargetInfo> subtargetInfo(target->createMCSubtargetInfo(targetTriple.str(), "", ""));

  // Create Machine Code Context from the triple.
  llvm::MCContext context(targetTriple, &*asmInfo, &*registerInfo, &*subtargetInfo);

  // Get the disassembler.
  std::unique_ptr<llvm::MCDisassembler> disasm(target->createMCDisassembler(*subtargetInfo, context));

  // Construct `ArrayRef` to feed the disassembler with.
  llvm::ArrayRef<uint8_t> bytes(reinterpret_cast<const uint8_t *>(pAssembledBytes), assembledBytesLength);

  bool result = true;

  std::vector<llvm::MCInst> decodedInstructions;

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
      decodedInstructions.push_back(retrievedInstruction);
      break;
    }

    i += instructionSize;
  }



  return result;
}

////////////////////////////////////////////////////////////////////////////////
