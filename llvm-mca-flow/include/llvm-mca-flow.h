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

#ifndef zydec_h__
#define zydec_h__

#include <cstdint>
#include <vector>
#include <string>
#include <tuple>
#include <optional>

////////////////////////////////////////////////////////////////////////////////

// resource index, resource usage
using ResourceUsageInfo = std::pair<size_t, double_t>;

struct InstructionInfo
{
  size_t instructionIndex, clockDispatched, clockExecuting, clockExecuted, clockRetired;
  std::vector<ResourceUsageInfo> usage;
  std::vector<std::string> bottleneckInfo;
};

struct PortUsageFlow
{
  std::vector<std::string> ports;
  std::vector<InstructionInfo> perClockInstruction;
};

////////////////////////////////////////////////////////////////////////////////

bool llvm_mca_flow_create(const void *pAssembledBytes, const size_t assembledBytesLength, PortUsageFlow *pFlow);

#endif // zydec_h__
