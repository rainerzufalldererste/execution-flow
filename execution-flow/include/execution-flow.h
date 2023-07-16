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

struct InstructionInfo
{
  size_t instructionIndex, instructionByteOffset, clockPending, clockReady, clockIssued, clockExecuted, clockDispatched, clockRetired, uOpCount;
  std::vector<ResourcePressureInfo> usage;
  std::vector<std::string> bottleneckInfo;
  std::vector<size_t> physicalRegistersObstructedPerRegisterType;

  inline InstructionInfo(const size_t instructionIndex, const size_t instructionByteOffset) :
    instructionIndex(instructionIndex),
    instructionByteOffset(instructionByteOffset),
    clockPending(0),
    clockReady(0),
    clockIssued(0),
    clockExecuted(0),
    clockDispatched(0),
    clockRetired(0),
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

bool execution_flow_create(const void *pAssembledBytes, const size_t assembledBytesLength, PortUsageFlow *pFlow, const size_t relevantIteration);

#endif // execution_flow_h__
