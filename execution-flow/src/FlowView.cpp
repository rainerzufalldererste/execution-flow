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

#include "FlowView.h"

#include "llvm/MCA/Support.h"

////////////////////////////////////////////////////////////////////////////////

void FlowView::onEvent(const llvm::mca::HWInstructionEvent &evnt)
{
  const size_t instructionCount = pFlow->instructionExecutionInfo.size();
  assert(instructionCount > 0 && "There should already be a reference to all instructions in this vector.");

  const size_t instructionIndex = evnt.IR.getSourceIndex() % instructionCount;
  const size_t runIndex = evnt.IR.getSourceIndex() / instructionCount;

  InstructionInfo &instructionInfo = pFlow->instructionExecutionInfo[instructionIndex];

  if (!hasFirstObservedInstructionClock)
  {
    hasFirstObservedInstructionClock = true;
    firstObservedInstructionClock = instructionClock;
  }

  switch (evnt.Type)
  {
  case llvm::mca::HWInstructionEvent::Dispatched:
  {
    if (runIndex == relevantIteration)
    {
      const auto &dispatchedEvent = static_cast<const llvm::mca::HWInstructionDispatchedEvent &>(evnt);

      instructionInfo.clockDispatched = instructionClock - firstObservedInstructionClock;
      instructionInfo.uOpCount = dispatchedEvent.MicroOpcodes;

      for (size_t i = 0; i < dispatchedEvent.UsedPhysRegs.size(); i++)
      {
        assert(i < isRegisterFileRelevant.size() && "More register files used than previously added");

        if (!isRegisterFileRelevant[i]) // Skip ones that were empty.
          continue;

        instructionInfo.physicalRegistersObstructedPerRegisterType.push_back((size_t)dispatchedEvent.UsedPhysRegs[i]);
      }
    }

    if (instructionInfo.perIteration.size() <= runIndex)
      instructionInfo.perIteration.resize(runIndex + 1);

    instructionInfo.perIteration[runIndex].clockDispatched = instructionClock;

    // Keep this instruction in-flight till it's been executed.
    inFlightInstructions.insert(std::make_pair(std::make_pair(runIndex, instructionIndex), true));

    break;
  }

  case llvm::mca::HWInstructionEvent::Ready:
  {
    if (runIndex == relevantIteration)
      instructionInfo.clockReady = instructionClock - firstObservedInstructionClock;

    if (instructionInfo.perIteration.size() <= runIndex)
      instructionInfo.perIteration.resize(runIndex + 1);

    instructionInfo.perIteration[runIndex].clockReady = instructionClock;

    break;
  }

  case llvm::mca::HWInstructionEvent::Executed:
  {
    if (runIndex == relevantIteration)
      instructionInfo.clockExecuted = instructionClock - firstObservedInstructionClock;

    if (instructionInfo.perIteration.size() <= runIndex)
      instructionInfo.perIteration.resize(runIndex + 1);

    instructionInfo.perIteration[runIndex].clockExecuted = instructionClock;

    inFlightInstructions.erase(std::make_pair(runIndex, instructionIndex));

    break;
  }

  case llvm::mca::HWInstructionEvent::Pending:
  {
    if (runIndex == relevantIteration)
      instructionInfo.clockPending = instructionClock - firstObservedInstructionClock;

    if (instructionInfo.perIteration.size() <= runIndex)
      instructionInfo.perIteration.resize(runIndex + 1);

    instructionInfo.perIteration[runIndex].clockPending = instructionClock;
    break;
  }

  case llvm::mca::HWInstructionEvent::Retired:
  {
    const auto &retiredEvent = static_cast<const llvm::mca::HWInstructionRetiredEvent &>(evnt);
    (void)retiredEvent; // seems to only contain the same registers that the dispatched event already contained, marking them as free.

    if (runIndex == relevantIteration)
      instructionInfo.clockRetired = instructionClock - firstObservedInstructionClock;

    if (instructionInfo.perIteration.size() <= runIndex)
      instructionInfo.perIteration.resize(runIndex + 1);

    instructionInfo.perIteration[runIndex].clockRetired = instructionClock;

    break;
  }

  case llvm::mca::HWInstructionEvent::Issued:
  {
    const auto &issuedEvent = static_cast<const llvm::mca::HWInstructionIssuedEvent &>(evnt);

    if (runIndex == relevantIteration)
      instructionInfo.clockIssued = instructionClock - firstObservedInstructionClock;

    if (instructionInfo.perIteration.size() <= runIndex)
      instructionInfo.perIteration.resize(runIndex + 1);

    instructionInfo.perIteration[runIndex].clockIssued = instructionClock;

    for (const auto &resourceUsage : issuedEvent.UsedResources)
    {
      if (!llvmResource2ListedResourceIdx.contains(resourceUsage.first))
      {
        assert(false && "The resource lookup doesn't contain this resource.");
        continue;
      }

      const size_t portIndex = llvmResource2ListedResourceIdx[resourceUsage.first];

      if (runIndex == relevantIteration)
        instructionInfo.usage.push_back(ResourcePressureInfo(portIndex, (double)resourceUsage.second));

      instructionInfo.perIteration[runIndex].usage.push_back(ResourcePressureInfo(portIndex, (double)resourceUsage.second));
    }

    const llvm::mca::Instruction *pInstruction = evnt.IR.getInstruction();

    // Handle Resource Pressure.
    {
      const uint64_t criticalResources = pInstruction->getCriticalResourceMask();

      for (size_t bit = 0; bit < 64; bit++)
      {
        const uint64_t mask = (uint64_t)1 << bit;

        if (!(criticalResources & mask))
          continue;

        const uint32_t parentResourceIndex = llvm::mca::getResourceStateIndex(mask);
        const llvm::MCProcResourceDesc *pResource = schedulerModel.getProcResource(parentResourceIndex);

        size_t internalFirstPortIndex = (size_t)-1;
        size_t internalResourceType = (size_t)-1;
        const llvm::mca::ResourceRef resourceReference(parentResourceIndex, 1);

        if (llvmResource2ListedResourceIdx.contains(resourceReference))
        {
          internalFirstPortIndex = llvmResource2ListedResourceIdx[resourceReference];
          internalResourceType = pFlow->ports[internalFirstPortIndex].resourceTypeIndex;
        }

        addResourcePressure(instructionInfo, runIndex, internalFirstPortIndex, internalResourceType, pResource->Name, *pInstruction);
      }
    }

    const llvm::mca::CriticalDependency &regDep = pInstruction->getCriticalRegDep();
    const llvm::mca::CriticalDependency &memDep = pInstruction->getCriticalMemDep();

    break;
  }
  }
}

void FlowView::onEvent(const llvm::mca::HWStallEvent &evnt)
{
  const size_t instructionCount = pFlow->instructionExecutionInfo.size();
  assert(instructionCount > 0 && "There should already be a reference to all instructions in this vector.");

  const size_t instructionIndex = evnt.IR.getSourceIndex() % instructionCount;
  const size_t runIndex = evnt.IR.getSourceIndex() / instructionCount;

  InstructionInfo &instructionInfo = pFlow->instructionExecutionInfo[instructionIndex];

  switch (evnt.Type)
  {
  case llvm::mca::HWStallEvent::RegisterFileStall:
    instructionInfo.stallInfo.emplace_back(std::string("Stall in Loop ") + std::to_string(runIndex) + ": Register Unavailable");
    break;

  case llvm::mca::HWStallEvent::RetireControlUnitStall:
    instructionInfo.stallInfo.emplace_back(std::string("Stall in Loop ") + std::to_string(runIndex) + ": Retire Tokens Unavailable");
    break;

  case llvm::mca::HWStallEvent::DispatchGroupStall:
    instructionInfo.stallInfo.emplace_back(std::string("Stall in Loop ") + std::to_string(runIndex) + ": Static Restrictions on the Dispatch Group");
    break;

  case llvm::mca::HWStallEvent::SchedulerQueueFull:
    instructionInfo.stallInfo.emplace_back(std::string("Stall in Loop ") + std::to_string(runIndex) + ": Scheduler Queue Full");
    break;

  case llvm::mca::HWStallEvent::LoadQueueFull:
    instructionInfo.stallInfo.emplace_back(std::string("Stall in Loop ") + std::to_string(runIndex) + ": Load Queue Full");
    break;

  case llvm::mca::HWStallEvent::StoreQueueFull:
    instructionInfo.stallInfo.emplace_back(std::string("Stall in Loop ") + std::to_string(runIndex) + ": Store Queue Full");
    break;

  case llvm::mca::HWStallEvent::CustomBehaviourStall:
    instructionInfo.stallInfo.emplace_back(std::string("Stall in Loop ") + std::to_string(runIndex) + ": Structural Hazard");
    break;
  }
}

void FlowView::onEvent(const llvm::mca::HWPressureEvent &evnt)
{
  const size_t instructionCount = pFlow->instructionExecutionInfo.size();
  assert(instructionCount > 0 && "There should already be a reference to all instructions in this vector.");

  for (const llvm::mca::InstRef &_inst : evnt.AffectedInstructions)
  {
    const size_t instructionIndex = _inst.getSourceIndex() & instructionCount;
    const size_t runIndex = _inst.getSourceIndex() / instructionCount;

    InstructionInfo &instruction = pFlow->instructionExecutionInfo[instructionIndex];
    LoopInstructionInfo &occurence = instruction.perIteration[runIndex];

    switch (evnt.Reason)
    {
    case llvm::mca::HWPressureEvent::RESOURCES:
      // TODO: Only apply pressure for cases where mask matches the instructions critical mask.
      occurence.resourcePressure.totalPressureCycles++;
      break;

    case llvm::mca::HWPressureEvent::REGISTER_DEPS:
      occurence.registerPressure.totalPressureCycles++;
      break;

    case llvm::mca::HWPressureEvent::MEMORY_DEPS:
      occurence.memoryPressure.totalPressureCycles++;
      break;
    }
  }
}

void FlowView::addLLVMResourceToPortIndexLookup(const std::pair<std::pair<size_t, size_t>, size_t> &keyValuePair)
{
  llvmResource2ListedResourceIdx.insert(keyValuePair);
}

void FlowView::addRegisterFileRelevancy(const bool isRelevant)
{
  isRegisterFileRelevant.push_back(isRelevant);
}

////////////////////////////////////////////////////////////////////////////////

void FlowView::addResourcePressure(InstructionInfo &info, const size_t iterationIndex, const size_t firstMatchingPortIndex, const size_t resourceType, const std::string &resourceName, const llvm::mca::Instruction &instruction)
{
  const size_t instructionCount = pFlow->instructionExecutionInfo.size();
  ResourceDependencyInfo &pressureContainer = info.perIteration[iterationIndex].resourcePressure;

  // TODO: Do something to distinguish between *confirmed* and *unconfirmed* dependencies (that caused or didn't cause pressure).
  pressureContainer.associatedResources.emplace_back(resourceType, firstMatchingPortIndex, resourceName);

  if (lastResourceUser.size() < pFlow->ports.size())
    lastResourceUser.resize(pFlow->ports.size(), std::make_pair((size_t)-1, (size_t)-1));

  if (lastResourceUser[resourceType].first != (size_t)-1)
  {
    ResourceTypeDependencyInfo &dependency = pressureContainer.associatedResources.back();
    dependency.origin = DependencyOrigin(lastResourceUser[resourceType].first, lastResourceUser[resourceType].second);
  }

  lastResourceUser[resourceType] = std::make_pair(iterationIndex, info.instructionIndex);
}
