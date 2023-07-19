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

#ifndef execution_flow_h__
#define execution_flow_h__

#include <cstdint>
#include <vector>
#include <string>
#include <tuple>
#include <optional>

////////////////////////////////////////////////////////////////////////////////

enum class CoreArchitecture
{
  _CurrentCPU,

  Alderlake,
  Broadwell,
  Cannonlake,
  Cascadelake,
  Cooperlake,
  EmeraldRapids,
  Goldmont,
  GoldmontPlus,
  GrandRidge,
  GraniteRapids,
  Haswell,
  IcelakeClient,
  IcelakeServer,
  IvyBridge,
  Meteorlake,
  Raptorlake,
  Rocketlake,
  Sandybridge,
  SapphireRapids,
  Sierraforest,
  Silvermont,
  SkylakeClient,
  SkylakeX,
  SkylakeServer,
  Tigerlake,
  Tremont,
  Zen1,
  Zen2,
  Zen3,
  Zen4,

  _Size
};

////////////////////////////////////////////////////////////////////////////////

struct ResourceInfo
{
  size_t resourceTypeIndex, resourceTypeSubIndex;
  std::string name;

  inline ResourceInfo(const size_t type, const size_t subIndex, const std::string name) :
    resourceTypeIndex(type),
    resourceTypeSubIndex(subIndex),
    name(name)
  { }
};

struct ResourcePressureInfo
{
  size_t resourceIndex;
  double_t pressure;

  inline ResourcePressureInfo(const size_t index, const double_t pressure) :
    resourceIndex(index),
    pressure(pressure)
  { }
};

struct HardwareRegisterCount
{
  std::string registerTypeName;
  size_t count;

  inline HardwareRegisterCount(const std::string type, const size_t count) :
    registerTypeName(type),
    count(count)
  { }
};

struct BasicInstructionInfo
{
  size_t clockPending, clockReady, clockIssued, clockExecuted, clockDispatched, clockRetired;
  std::vector<ResourcePressureInfo> usage;

  inline BasicInstructionInfo() :
    clockPending(0),
    clockReady(0),
    clockIssued(0),
    clockExecuted(0),
    clockDispatched(0),
    clockRetired(0)
  { }
};

struct DependencyOrigin
{
  size_t iterationIndex, instructionIndex;

  inline DependencyOrigin(const size_t iteration, const size_t instruction) :
    iterationIndex(iteration),
    instructionIndex(instruction)
  { }
};

struct ResourceTypeDependencyInfo
{
  size_t resourceTypeIndex; // if this is -1 we don't have the resource / resource type in ports.
  size_t firstMatchingPortIndex; // the first port with the given resource type. (there may be multiple ports with this resource type)
  std::string resourceName;
  size_t pressureCycles;
  std::optional<DependencyOrigin> origin;

  inline ResourceTypeDependencyInfo(const size_t resourceType, const size_t matchingPort, const std::string &name) :
    resourceTypeIndex(resourceType),
    firstMatchingPortIndex(matchingPort),
    resourceName(name),
    pressureCycles(0)
  { }
};

struct DependencyInfo
{
  size_t totalPressureCycles; // may have accumulated over multiple dependencies.
  size_t selfPressureCycles; // just the ones for this dependency.
  std::optional<DependencyOrigin> origin;
  
  inline DependencyInfo() :
    totalPressureCycles(0),
    selfPressureCycles(0)
  { }
};

struct RegisterDependencyInfo : DependencyInfo
{
  std::string registerName;

  inline RegisterDependencyInfo() :
    DependencyInfo()
  { }
};

struct ResourceDependencyInfo
{
  size_t totalPressureCycles; // may have accumulated over multiple dependencies.
  std::vector<ResourceTypeDependencyInfo> associatedResources;

  inline ResourceDependencyInfo() :
    totalPressureCycles(0)
  { }
};

struct LoopInstructionInfo : BasicInstructionInfo
{
  size_t totalPressureCycles;
  RegisterDependencyInfo registerPressure;
  ResourceDependencyInfo resourcePressure;
  DependencyInfo memoryPressure;

  inline LoopInstructionInfo() :
    totalPressureCycles(0)
  { }
};

struct InstructionInfo : BasicInstructionInfo
{
  size_t instructionIndex, instructionByteOffset, uOpCount;
  std::vector<std::string> stallInfo;
  std::vector<size_t> physicalRegistersObstructedPerRegisterType;
  std::vector<LoopInstructionInfo> perIteration;

  inline InstructionInfo(const size_t instructionIndex, const size_t instructionByteOffset) :
    BasicInstructionInfo(),
    instructionIndex(instructionIndex),
    instructionByteOffset(instructionByteOffset),
    uOpCount(0)
  { }
};

struct PortUsageFlow
{
  std::vector<ResourceInfo> ports;
  std::vector<HardwareRegisterCount> hardwareRegisters;
  std::vector<InstructionInfo> instructionExecutionInfo;
};

////////////////////////////////////////////////////////////////////////////////

bool execution_flow_create(const void *pAssembledBytes, const size_t assembledBytesLength, PortUsageFlow *pFlow, const CoreArchitecture arch, const size_t iterations, const size_t relevantIteration);

#endif // execution_flow_h__
