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

#ifndef FlowView_h__
#define FlowView_h__

#include "execution-flow.h"

#ifdef _MSC_VER
#pragma warning (push, 0)
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#endif
#include "llvm/ADT/DenseMap.h"
#include "llvm/MC/MCInstPrinter.h"
#include "llvm/MCA/HWEventListener.h"
#ifdef _MSC_VER
#pragma warning (pop)
#else
#pragma GCC diagnostic pop
#endif

////////////////////////////////////////////////////////////////////////////////

class FlowView final : public llvm::mca::HWEventListener
{
private:
  PortUsageFlow *pFlow; // initialized in the constructor.
  size_t relevantIteration; // initialized in the constructor.
  size_t instructionClock = 0;
  llvm::SmallDenseMap<std::pair<uint64_t, uint64_t>, size_t, 32U> llvmResource2ListedResourceIdx;
  bool hasFirstObservedInstructionClock = false;
  size_t firstObservedInstructionClock = 0;
  llvm::SmallVector<bool> isRegisterFileRelevant;
  const llvm::MCSchedModel &schedulerModel; // initialized in the constructor.
  const llvm::MCInstPrinter &instructionPrinter; // initialized in the constructor.

  // llvmResourceIndex => (runIndex, instructionIndex).
  llvm::SmallVector<std::pair<size_t, size_t>> lastResourceUser;
  llvm::SmallVector<std::pair<size_t, size_t>> preLastResourceUser; // sometimes the resource has already been made the current resource, so we'll also have the one before that as a backup.

  // TODO: this should be a pool, not a map.
  llvm::DenseMap<std::pair<size_t, size_t>, bool> inFlightInstructions; // (runIndex, instruction index), bool is meaningless.

  void addResourcePressure(InstructionInfo &info, const size_t iterationIndex, const size_t llvmResourceMask, const llvm::mca::Instruction &instruction, const bool fromPressureEvent);
  void addRegisterPressure(InstructionInfo &info, const size_t selfIterationIndex, const size_t dependencyIterationIndex, const size_t dependencyInstructionIndex, const llvm::MCPhysReg &physicalRegister, const size_t dependencyCycles);
  void addMemoryPressure(InstructionInfo &info, const size_t selfIterationIndex, const size_t dependencyIterationIndex, const size_t dependencyInstructionIndex, const size_t dependencyCycles);

public:
  inline FlowView(PortUsageFlow *pFlow, const llvm::MCSchedModel &schedulerModel, const llvm::MCInstPrinter &instructionPrinter, const size_t relevantIteration) :
    pFlow(pFlow),
    relevantIteration(relevantIteration),
    schedulerModel(schedulerModel),
    instructionPrinter(instructionPrinter)
  { }

  inline void onCycleEnd() override { instructionClock++; }

  void onEvent(const llvm::mca::HWInstructionEvent &evnt) override;
  void onEvent(const llvm::mca::HWStallEvent &evnt) override;
  void onEvent(const llvm::mca::HWPressureEvent &evnt) override;

  void addLLVMResourceToPortIndexLookup(const std::pair<std::pair<size_t, size_t>, size_t> &keyValuePair);
  void addRegisterFileRelevancy(const bool isRelevant);
};

#endif // FlowView_h__
