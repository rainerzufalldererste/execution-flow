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
    
    // Handle Resource Dependency.
    {
      const uint64_t criticalResources = pInstruction->getCriticalResourceMask();
    
      for (size_t bit = 0; bit < 64; bit++)
      {
        const uint64_t mask = (uint64_t)1 << bit;
    
        if (!(criticalResources & mask))
          continue;
    
        addResourcePressure(instructionInfo, runIndex, llvm::mca::getResourceStateIndex(mask), *pInstruction, false);
      }
    }
    
    // Handle Register Dependency.
    {
      const llvm::mca::CriticalDependency &registerDependency = pInstruction->getCriticalRegDep();
    
      if (registerDependency.Cycles != 0)
        addRegisterPressure(instructionInfo, runIndex, (size_t)registerDependency.IID / instructionCount, (size_t)registerDependency.IID % instructionCount, registerDependency.RegID, registerDependency.Cycles);
    }
    
    // Handle Memory Dependency.
    {
      const llvm::mca::CriticalDependency &memoryDependency = pInstruction->getCriticalMemDep();
    
      if (memoryDependency.Cycles != 0)
        addMemoryPressure(instructionInfo, runIndex, (size_t)memoryDependency.IID / instructionCount, (size_t)memoryDependency.IID % instructionCount, memoryDependency.Cycles);
    }

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
    const size_t instructionIndex = _inst.getSourceIndex() % instructionCount;
    const size_t runIndex = _inst.getSourceIndex() / instructionCount;
  
    InstructionInfo &instructionInfo = pFlow->instructionExecutionInfo[instructionIndex];
    
    if (instructionInfo.perIteration.size() <= runIndex)
      instructionInfo.perIteration.resize(runIndex + 1);
  
    LoopInstructionInfo &occurence = instructionInfo.perIteration[runIndex];
  
    switch (evnt.Reason)
    {
    case llvm::mca::HWPressureEvent::RESOURCES:
    {
      occurence.resourcePressure.totalPressureCycles++;
  
      const llvm::mca::Instruction *pMcaInstruction = _inst.getInstruction();
      const size_t criticalResources = pMcaInstruction->getCriticalResourceMask() & evnt.ResourceMask;
  
      for (size_t bit = 0; bit < 64; bit++)
      {
        const uint64_t mask = (uint64_t)1 << bit;
  
        if (!(criticalResources & mask))
          continue;
  
        addResourcePressure(instructionInfo, runIndex, llvm::mca::getResourceStateIndex(mask), *pMcaInstruction, true);
      }
  
      break;
    }
  
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

void FlowView::addResourcePressure(InstructionInfo &info, const size_t iterationIndex, const size_t llvmResourceIndex, const llvm::mca::Instruction &instruction, const bool fromPressureEvent)
{
  const llvm::MCProcResourceDesc *pResource = schedulerModel.getProcResource(llvmResourceIndex);
  
  size_t firstMatchingPortIndex = (size_t)-1;
  size_t resourceType = (size_t)-1;
  const llvm::mca::ResourceRef resourceReference(llvmResourceIndex, 1);
  
  if (llvmResource2ListedResourceIdx.contains(resourceReference))
  {
    firstMatchingPortIndex = llvmResource2ListedResourceIdx[resourceReference];
    resourceType = pFlow->ports[firstMatchingPortIndex].resourceTypeIndex;
  }
  else if (pResource->NumUnits > 0 && pResource->SubUnitsIdxBegin != nullptr)
  {
    for (size_t i = 0; i < pResource->NumUnits; i++)
      addResourcePressure(info, iterationIndex, pResource->SubUnitsIdxBegin[i], instruction, fromPressureEvent);

    return;
  }
  
  ResourceDependencyInfo &pressureContainer = info.perIteration[iterationIndex].resourcePressure;
  ResourceTypeDependencyInfo *pDependency = nullptr;
  
  for (auto &_dep : pressureContainer.associatedResources)
  {
    if (_dep.resourceTypeIndex == resourceType)
    {
      pDependency = &_dep;
      break;
    }
  }
  
  if (pDependency == nullptr)
  {
    pressureContainer.associatedResources.emplace_back(resourceType, firstMatchingPortIndex, pResource->Name);
    pDependency = &pressureContainer.associatedResources.back();
  }
  
  if (fromPressureEvent)
  {
    pDependency->pressureCycles++;
  }
  else
  {
    if (lastResourceUser.size() <= llvmResourceIndex)
      lastResourceUser.resize(llvmResourceIndex + 1, std::make_pair((size_t)-1, (size_t)-1));
  
    if (lastResourceUser[llvmResourceIndex].first != (size_t)-1)
      pDependency->origin = DependencyOrigin(lastResourceUser[llvmResourceIndex].first, lastResourceUser[llvmResourceIndex].second);
  
    lastResourceUser[llvmResourceIndex] = std::make_pair(iterationIndex, info.instructionIndex);
  }
}

void FlowView::addRegisterPressure(InstructionInfo &info, const size_t selfIterationIndex, const size_t dependencyIterationIndex, const size_t dependencyInstructionIndex, const llvm::MCPhysReg &physicalRegister, const size_t dependencyCycles)
{
  RegisterDependencyInfo &pressureContainer = info.perIteration[selfIterationIndex].registerPressure;

  pressureContainer.selfPressureCycles = dependencyCycles;
  pressureContainer.origin = std::make_optional<DependencyOrigin>(dependencyIterationIndex, dependencyInstructionIndex);

  llvm::raw_string_ostream stringStream(pressureContainer.registerName);
  instructionPrinter.printRegName(stringStream, physicalRegister);
}

void FlowView::addMemoryPressure(InstructionInfo &info, const size_t selfIterationIndex, const size_t dependencyIterationIndex, const size_t dependencyInstructionIndex, const size_t dependencyCycles)
{
  DependencyInfo &pressureContainer = info.perIteration[selfIterationIndex].memoryPressure;

  pressureContainer.selfPressureCycles = dependencyCycles;
  pressureContainer.origin = std::make_optional<DependencyOrigin>(dependencyIterationIndex, dependencyInstructionIndex);
}
