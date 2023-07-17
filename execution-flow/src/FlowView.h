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

#pragma warning (push, 0)
#include "llvm/ADT/DenseMap.h"
#include "llvm/MCA/HWEventListener.h"
#pragma warning (pop)

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

public:
  inline FlowView(PortUsageFlow *pFlow, const size_t relevantIteration) :
    pFlow(pFlow),
    relevantIteration(relevantIteration)
  { }

  inline void onCycleEnd() override { instructionClock++; }

  void onEvent(const llvm::mca::HWInstructionEvent &evnt) override;
  void onEvent(const llvm::mca::HWStallEvent &evnt) override;
  void onEvent(const llvm::mca::HWPressureEvent &evnt) override;

  void addLLVMResourceToPortIndexLookup(const std::pair<std::pair<size_t, size_t>, size_t> &keyValuePair);
  void addRegisterFileRelevancy(const bool isRelevant);
};

#endif // FlowView_h__
