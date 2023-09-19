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

#include "execution-flow.h"
#include "html_static.h"

extern "C"
{
#define ZYDIS_STATIC_BUILD
#include "Zydis.h"
}

#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>

////////////////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
#define DBG_BREAK() __debugbreak()
#else
#define DBG_BREAK()
#endif

#define FATAL(x, ...) do { printf(x "\n", __VA_ARGS__); DBG_BREAK(); exit(-1); } while (0)
#define FATAL_IF(conditional, x, ...) do { if (conditional) { FATAL(x, __VA_ARGS__); } } while (0)

static const char *_ArgumentTargetCpu = "-march";
static const char *_ArgumentIterations = "-iter";

////////////////////////////////////////////////////////////////////////////////

static const char *TargetLookup[] =
{
  nullptr,

  "Alderlake",
  "Broadwell",
  "Cannonlake",
  "Cascadelake",
  "Cooperlake",
  "EmeraldRapids",
  "Goldmont",
  "GoldmontPlus",
  "GrandRidge",
  "GraniteRapids",
  "Haswell",
  "IcelakeClient",
  "IcelakeServer",
  "IvyBridge",
  "Meteorlake",
  "Raptorlake",
  "Rocketlake",
  "Sandybridge",
  "SapphireRapids",
  "Sierraforest",
  "Silvermont",
  "SkylakeClient",
  "SkylakeX",
  "SkylakeServer",
  "Tigerlake",
  "Tremont",
  "Zen1",
  "Zen2",
  "Zen3",
  "Zen4",
};

static_assert(std::size(TargetLookup) == (size_t)CoreArchitecture::_Count);

////////////////////////////////////////////////////////////////////////////////

#pragma optimize("", off)

int main(int argc, char **pArgv)
{
  if (argc < 3)
  {
    puts("Usage: execution-flow-html <RawAssembledBinaryFile> <AnalysisFile.html>");
    puts("\n\t Optional Parameters:\n");
    
    printf("\t\t%s <target cpu core architecture> (defaults to current cpu if not specified)\n", _ArgumentTargetCpu);
    
    for (size_t i = 1; i < (size_t)CoreArchitecture::_Count; i++)
      printf("\t\t\t%s\n", TargetLookup[i]);

    puts("");
    printf("\t\t%s <number of iterations to simulate>\n\t\t\t", _ArgumentIterations);

    return 0;
  }

  const char *inFilename = pArgv[1];
  const char *outFilename = pArgv[2];
  CoreArchitecture targetCpu = CoreArchitecture::_CurrentCPU;
  size_t loopIterations = 8;

  for (size_t argIdx = 3; argIdx < (size_t)argc; /* Iterated manually, as arg sizes are context dependent. */)
  {
    const size_t argsRemaining = (size_t)argc - argIdx;

    if (argsRemaining >= 2 && strncmp(_ArgumentIterations, pArgv[argIdx], sizeof(_ArgumentIterations)) == 0)
    {
      loopIterations = strtoull(pArgv[argIdx + 1], nullptr, 10);

      if (loopIterations == 0)
      {
        printf("Invalid iteration count '%s'. Aborting.\n", pArgv[argIdx + 1]);
        return EXIT_FAILURE;
      }

      argIdx += 2;
    }
    else if (argsRemaining >= 2 && strncmp(_ArgumentTargetCpu, pArgv[argIdx], sizeof(_ArgumentTargetCpu)) == 0)
    {
      for (size_t i = 1; i < (size_t)CoreArchitecture::_Count; i++)
      {
        if (strcmp(TargetLookup[i], pArgv[argIdx + 1]) == 0)
        {
          targetCpu = (CoreArchitecture)i;
          break;
        }
      }

      if (targetCpu == CoreArchitecture::_CurrentCPU)
      {
        printf("Invalid target cpu core architecture '%s'. Aborting.\n", pArgv[argIdx + 1]);
        return EXIT_FAILURE;
      }

      argIdx += 2;
    }
    else
    {
      printf("Unexpected parameter '%s'. Aborting.\n", pArgv[argIdx]);
      return EXIT_FAILURE;
    }
  }

  uint8_t *pData = nullptr;
  size_t fileSize = 0;

  // Read the input file.
  {
    FILE *pInFile = fopen(inFilename, "rb");
    FATAL_IF(pInFile == nullptr, "Failed to open file. Aborting.");

    fseek(pInFile, 0, SEEK_END);
    fileSize = _ftelli64(pInFile);
    FATAL_IF(fileSize == 0, "The specified file is empty. Aborting.");

    fseek(pInFile, 0, SEEK_SET);

    pData = reinterpret_cast<uint8_t *>(malloc(fileSize));
    FATAL_IF(pData == nullptr, "Memory allocation failure. Aborting.");
    FATAL_IF(fileSize != fread(pData, 1, fileSize, pInFile), "Failed to read file contents. Aborting.");

    fclose(pInFile);
  }

  // Create flow.
  PortUsageFlow flow;
  const bool result = execution_flow_create(pData, fileSize, &flow, targetCpu, loopIterations, 0);

  if (!result)
    puts("Failed to create port usage flow correctly. This could mean that the provided file wasn't valid.");

  printf("%" PRIu64 " Instructions decoded.\n", flow.instructionExecutionInfo.size());

  if (flow.instructionExecutionInfo.size() == 0)
  {
    puts("Aborting.");
    return 1;
  }

  // Write HTML Flow.
  {
    FILE *pOutFile = fopen(outFilename, "w");
    FATAL_IF(pOutFile == nullptr, "Failed to create output file. Aborting.");

    fputs(_HtmlDocumentSetup, pOutFile);
    fprintf(pOutFile, "<style>\n:root {--lane-count: %" PRIu64 ";\n}\n</style>", flow.ports.size());

    std::vector<std::string> disassemblyLines;

    // Add disassembly.
    {
      fputs("<div class=\"disasmcontainer\">\n<div class=\"disasm\">\n", pOutFile);

      ZydisDecoder decoder;
      ZydisFormatter formatter;

      FATAL_IF(!ZYAN_SUCCESS(ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64)), "Failed to initialize disassembler.");
      FATAL_IF(!ZYAN_SUCCESS(ZydisFormatterInit(&formatter, ZYDIS_FORMATTER_STYLE_INTEL)) || !ZYAN_SUCCESS(ZydisFormatterSetProperty(&formatter, ZYDIS_FORMATTER_PROP_FORCE_SEGMENT, ZYAN_TRUE)) || !ZYAN_SUCCESS(ZydisFormatterSetProperty(&formatter, ZYDIS_FORMATTER_PROP_FORCE_SIZE, ZYAN_TRUE)), "Failed to initialize instruction formatter.");

      ZydisDecodedInstruction instruction;
      ZydisDecodedOperand operands[10];

      size_t virtualAddress = 0;
      constexpr size_t addressDisplayOffset = 0x140000000;

      char disasmBuffer[1024] = "";
      size_t instructionIndex = (size_t)-1;

      while (virtualAddress < fileSize)
      {
        ++instructionIndex;

        FATAL_IF(!(ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, pData + virtualAddress, fileSize - virtualAddress, &instruction, operands))), "Invalid Instruction at 0x%" PRIX64 ".", virtualAddress);
        FATAL_IF(!ZYAN_SUCCESS(ZydisFormatterFormatInstruction(&formatter, &instruction, operands, sizeof(operands) / sizeof(operands[0]), disasmBuffer, sizeof(disasmBuffer), virtualAddress + addressDisplayOffset, nullptr)), "Failed to Format Instruction at 0x%" PRIX64 ".", virtualAddress);

        disassemblyLines.push_back(disasmBuffer);

        const auto &instructionInfo = flow.instructionExecutionInfo[instructionIndex];

        const char *subVariant = instructionInfo.stallInfo.size() > 0 ? " highlighted" : (instructionInfo.usage.size() == 0 && instructionInfo.clockExecuted - instructionInfo.clockIssued == 0 ? " null" : "");

        fprintf(pOutFile, "<div class=\"disasmline\" idx=\"%" PRIu64 "\"><span class=\"linenum%s\">0x%08" PRIX64 "&emsp;</span><span class=\"asm%s\" style=\"--exec: %" PRIu64 ";\">%s</span>", instructionIndex, subVariant, virtualAddress + addressDisplayOffset, subVariant, instructionInfo.clockExecuted - instructionInfo.clockIssued, disasmBuffer);

        size_t dispatched = 0;
        size_t pending = 0;
        size_t ready = 0;
        size_t executing = 0;
        size_t retiring = 0;
        const size_t iterations = instructionInfo.perIteration.size();

        // Add Dependency Arrows & Calculate Average Clocks.
        {
          for (size_t iteration = 0; iteration < iterations; iteration++)
          {
            const auto &it = instructionInfo.perIteration[iteration];
           
            dispatched += it.clockPending - it.clockDispatched;
            pending += it.clockReady - it.clockPending;
            ready += it.clockIssued - it.clockReady;
            executing += it.clockExecuted - it.clockIssued;
            retiring += it.clockRetired - it.clockExecuted;
            
            const auto &regP = it.registerPressure;

            if (regP.selfPressureCycles > 0 && regP.origin.has_value() && regP.origin.value().iterationIndex != (size_t)-1)
              fprintf(pOutFile, "<div class=\"depptr register\" style=\"--e: %" PRIi64 "\"></div>", (int64_t)instructionInfo.instructionIndex - regP.origin.value().instructionIndex);

            const auto &memP = it.memoryPressure;

            if (memP.selfPressureCycles > 0 && memP.origin.has_value() && memP.origin.value().iterationIndex != (size_t)-1)
              fprintf(pOutFile, "<div class=\"depptr memory\" style=\"--e: %" PRIi64 "\"></div>", (int64_t)instructionInfo.instructionIndex - memP.origin.value().instructionIndex);

            const auto &rsrcP = it.resourcePressure;

            for (const auto &_port : rsrcP.associatedResources)
              if (_port.pressureCycles > 0 && _port.origin.has_value())
                fprintf(pOutFile, "<div class=\"depptr resource\" style=\"--e: %" PRIi64 "\"></div>", (int64_t)instructionInfo.instructionIndex - _port.origin.value().instructionIndex);
          }
        }

        fputs("<div class=\"extra_info\">", pOutFile);

        const double iterationsF = (double)iterations;

        fprintf(pOutFile, "<div class=\"uops\">%" PRIu64 " uOps</div>", instructionInfo.uOpCount);
        fprintf(pOutFile, "<div class=\"cycleInfo\">dispatched: %3.1f cycles</div>", dispatched / iterationsF);
        fprintf(pOutFile, "<div class=\"cycleInfo\">pending: %3.1f cycles</div>", pending / iterationsF);
        fprintf(pOutFile, "<div class=\"cycleInfo\">ready: %3.1f cycles</div>", ready / iterationsF);
        fprintf(pOutFile, "<div class=\"cycleInfo\">executing: %3.1f cycles</div>", executing / iterationsF);
        fprintf(pOutFile, "<div class=\"cycleInfo\">retiring: %3.1f cycles</div>", retiring / iterationsF);

        for (size_t j = 0; j < instructionInfo.physicalRegistersObstructedPerRegisterType.size(); j++)
        {
          if (instructionInfo.physicalRegistersObstructedPerRegisterType[j] == 0)
            continue;

          if (flow.hardwareRegisters.size() > j)
            fprintf(pOutFile, "<div class=\"registers\">%" PRIu64 " %s registers used (total: %" PRIu64 ")</div>", instructionInfo.physicalRegistersObstructedPerRegisterType[j], flow.hardwareRegisters[j].registerTypeName.c_str(), flow.hardwareRegisters[j].count);
          else
            fprintf(pOutFile, "<div class=\"registers\">%" PRIu64 " registers used</div>", instructionInfo.physicalRegistersObstructedPerRegisterType[j]);
        }

        if (instructionInfo.usage.size() != 0)
        {
          fputs("<div class=\"resourcecontainer\">\n", pOutFile);

          for (const auto &_rsrcIdx : instructionInfo.usage)
            fprintf(pOutFile, "<span class=\"rsrc\" style=\"--lane: %" PRIu64 "\">%s: %3.1f</span>", _rsrcIdx.resourceIndex, flow.ports[_rsrcIdx.resourceIndex].name.c_str(), _rsrcIdx.pressure);

          fputs("</div>\n", pOutFile);
        }

        // Add Stall Info.
        for (const auto &_b : instructionInfo.stallInfo)
          fprintf(pOutFile, "<div class=\"stall\">%s</div>", _b.c_str());

        // Add Dependency Info.
        {
          for (size_t iteration = 0; iteration < instructionInfo.perIteration.size(); iteration++)
          {
            const auto &regP = instructionInfo.perIteration[iteration].registerPressure;

            if (regP.selfPressureCycles > 0 && regP.origin.has_value() && regP.origin.value().iterationIndex != (size_t)-1)
              fprintf(pOutFile, "<div class=\"dependency register\">%" PRIu64 " cycle(s) on <span class=\"press_obj\">%s</span> <span class=\"loop\">%" PRIu64 "</span></div>", regP.selfPressureCycles, regP.registerName.c_str(), iteration);

            const auto &memP = instructionInfo.perIteration[iteration].memoryPressure;

            if (memP.selfPressureCycles > 0 && memP.origin.has_value() && memP.origin.value().iterationIndex != (size_t)-1)
              fprintf(pOutFile, "<div class=\"dependency memory\">%" PRIu64 " cycle(s) on memory <span class=\"loop\">%" PRIu64 "</span></div>", memP.selfPressureCycles, iteration);

            const auto &rsrcP = instructionInfo.perIteration[iteration].resourcePressure;

            for (const auto &_port : rsrcP.associatedResources)
            {
              if (_port.pressureCycles > 0 && _port.origin.has_value())
              {
                if (_port.origin.value().iterationIndex == iteration)
                  fprintf(pOutFile, "<div class=\"dependency resource\">%" PRIu64 " cycle(s) on <span class=\"press_obj\">%s</span> <span class=\"loop\" title=\"Loop Index\">%" PRIu64 "</span></div>", _port.pressureCycles, _port.resourceName.c_str(), iteration);
                else
                  fprintf(pOutFile, "<div class=\"dependency resource\">%" PRIu64 " cycle(s) on <span class=\"press_obj\">%s</span> <span class=\"loop\" title=\"Loop Index\">%" PRIu64 "</span> <span class=\"loop_origin\" title=\"Dependency Origin Loop Index\">%" PRIu64 "</span></div>", _port.pressureCycles, _port.resourceName.c_str(), iteration, _port.origin.value().iterationIndex);
              }
            }
          }
        }

        fputs("</div>\n<div class=\"dependency_data\">\n", pOutFile);

        // Add Dependency Info.
        {
          for (size_t iteration = 0; iteration < instructionInfo.perIteration.size(); iteration++)
          {
            const auto &regP = instructionInfo.perIteration[iteration].registerPressure;

            if (regP.selfPressureCycles > 0 && regP.origin.has_value() && regP.origin.value().iterationIndex != (size_t)-1)
              fprintf(pOutFile, "<div class=\"__reg\" cycles=\"%" PRIu64 "\" desc=\"%s\" iteration=\"%" PRIu64 "\" index=\"%" PRIu64 "\"></div>", regP.selfPressureCycles, regP.registerName.c_str(), regP.origin.value().iterationIndex, regP.origin.value().instructionIndex);

            const auto &memP = instructionInfo.perIteration[iteration].memoryPressure;

            if (memP.selfPressureCycles > 0 && memP.origin.has_value() && memP.origin.value().iterationIndex != (size_t)-1)
              fprintf(pOutFile, "<div class=\"__mem\" cycles=\"%" PRIu64 "\" iteration=\"%" PRIu64 "\" index=\"%" PRIu64 "\"></div>", memP.selfPressureCycles, memP.origin.value().iterationIndex, memP.origin.value().instructionIndex);

            const auto &rsrcP = instructionInfo.perIteration[iteration].resourcePressure;

            for (const auto &_port : rsrcP.associatedResources)
            {
              if (_port.pressureCycles > 0 && _port.origin.has_value())
              {
                const auto other = flow.instructionExecutionInfo[_port.origin.value().instructionIndex].perIteration[_port.origin.value().iterationIndex];

                for (const auto &_otherPort : other.usage)
                {
                  // if (flow.ports[_otherPort.resourceIndex].resourceTypeIndex == flow.ports[_port.firstMatchingPortIndex].resourceTypeIndex) // <- this doesn't match anything quite often, as apparently instructions have dependencies on resources they don't use and depend on instructions on that port, that also didn't use this resource.
                    fprintf(pOutFile, "<div class=\"__rsc\" cycles=\"%" PRIu64 "\" desc=\"%s\" iteration=\"%" PRIu64 "\" index=\"%" PRIu64 "\" lane=\"%" PRIu64 "\"></div>", _port.pressureCycles, _port.resourceName.c_str(), _port.origin.value().iterationIndex, _port.origin.value().instructionIndex, _otherPort.resourceIndex);
                }
              }
            }
          }
        }

        fputs("\n</div></div>\n", pOutFile);

        virtualAddress += instruction.length;
      }

      fputs("<div class=\"stats\">\n", pOutFile);

      for (size_t i = 0; i < loopIterations; i++)
      {
        fprintf(pOutFile, "<div class=\"stats_it\"><h2>Iteration %" PRIu64 "</h2>", i + 1);

        size_t earliestDispatch = (size_t)-1;
        size_t lastRetire = 0;
        size_t earliestIssued = (size_t)-1;
        size_t lastExecuted = 0;
        size_t totalDispatched = 0;
        size_t totalPending = 0;
        size_t totalReady = 0;
        size_t totalExecuting = 0;
        size_t totalRetiring = 0;

        // Calculate Averages & Bounds.
        for (const auto &_instruction : flow.instructionExecutionInfo)
        {
          if (_instruction.perIteration.size() <= i)
            continue;

          const auto &it = _instruction.perIteration[i];

          earliestDispatch = std::min(earliestDispatch, it.clockDispatched);
          lastRetire = std::max(lastRetire, it.clockRetired);

          earliestIssued = std::min(earliestIssued, it.clockIssued);
          lastExecuted = std::max(lastExecuted, it.clockExecuted);

          totalDispatched += it.clockPending - it.clockDispatched;
          totalPending += it.clockReady - it.clockPending;
          totalReady += it.clockIssued - it.clockReady;
          totalExecuting += it.clockExecuted - it.clockIssued;
          totalRetiring += it.clockRetired - it.clockExecuted;
        }

        std::vector<size_t> perPortUsage(flow.ports.size(), 0);
        std::vector<bool> portUsed(flow.ports.size(), false);

        // Check Utilization in Bounds.
        for (size_t cycle = earliestIssued; cycle < lastExecuted; cycle++)
        {
          for (size_t port = 0; port < portUsed.size(); port++)
            portUsed[port] = false;

          for (const auto &_instruction : flow.instructionExecutionInfo)
          {
            if (_instruction.perIteration.size() <= i)
              continue;

            const auto &it = _instruction.perIteration[i];

            // If active: Mark port as used.
            if (it.clockIssued <= cycle && it.clockExecuted > cycle)
              for (const auto &_port : _instruction.usage)
                portUsed[_port.resourceIndex] = true;
          }

          for (size_t port = 0; port < portUsed.size(); port++)
            if (portUsed[port])
              perPortUsage[port]++;
        }

        enum ExecutionState
        {
          ES_Dispatched,
          ES_Pending,
          ES_Ready,
          ES_Executing,
          ES_Retiring,

          _ES_Count
        };

        bool stateInUse[_ES_Count];
        size_t stateCyclesInUse[_ES_Count] = {};

        // Check States in Bounds.
        for (size_t cycle = earliestDispatch; cycle < lastRetire; cycle++)
        {
          for (size_t s = 0; s < _ES_Count; s++)
            stateInUse[s] = false;

          for (const auto &_instruction : flow.instructionExecutionInfo)
          {
            if (_instruction.perIteration.size() <= i)
              continue;

            const auto &it = _instruction.perIteration[i];

            // If active: Mark state as used.
            {
              if (it.clockDispatched <= cycle && it.clockPending > cycle)
                stateInUse[ES_Dispatched] = true;

              if (it.clockPending <= cycle && it.clockReady > cycle)
                stateInUse[ES_Pending] = true;

              if (it.clockReady <= cycle && it.clockIssued > cycle)
                stateInUse[ES_Ready] = true;

              if (it.clockIssued <= cycle && it.clockExecuted > cycle)
                stateInUse[ES_Executing] = true;

              if (it.clockExecuted <= cycle && it.clockRetired > cycle)
                stateInUse[ES_Retiring] = true;
            }
          }

          for (size_t s = 0; s < _ES_Count; s++)
            if (stateInUse[s])
              stateCyclesInUse[s]++;
        }

        fprintf(pOutFile, "<b>%" PRIu64 " Cycles Total (first dispatch -> last retire)</b><b>%" PRIu64 " Cycles (first issued -> last executed)</b>", lastRetire - earliestDispatch, lastExecuted - earliestIssued);
        fprintf(pOutFile, "<i>Dispatched: %" PRIu64 " distinct Cycles <i>(%" PRIu64 " total)</i></i>", stateCyclesInUse[ES_Dispatched], totalDispatched);
        fprintf(pOutFile, "<i>Pending: %" PRIu64 " distinct Cycles <i>(%" PRIu64 " total)</i></i>", stateCyclesInUse[ES_Pending], totalPending);
        fprintf(pOutFile, "<i>Ready: %" PRIu64 " distinct Cycles <i>(%" PRIu64 " total)</i></i>", stateCyclesInUse[ES_Ready], totalReady);
        fprintf(pOutFile, "<i>Executing: %" PRIu64 " distinct Cycles <i>(%" PRIu64 " total)</i></i>", stateCyclesInUse[ES_Executing], totalExecuting);
        fprintf(pOutFile, "<i>Retiring: %" PRIu64 " distinct Cycles <i>(%" PRIu64 " total)</i></i>", stateCyclesInUse[ES_Retiring], totalRetiring);

        for (size_t i = 0; i < perPortUsage.size(); i++)
          fprintf(pOutFile, "<i class=\"s\" style=\"--h:%1.4f;\">%s: %4.2f%%</i>", (double)perPortUsage[i] / (lastExecuted - earliestIssued), flow.ports[i].name.c_str(), (100.0 * perPortUsage[i]) / (lastExecuted - earliestIssued));

        fputs("</div>\n", pOutFile);
      }

      fputs("</div>\n", pOutFile);

      fputs("<div class=\"spacer\"></div></div>\n</div>\n", pOutFile);
    }

    // Add flow graph.
    {
      fputs("<div class=\"flowgraph\"><table class=\"flow\"><tr>\n", pOutFile);

      for (const auto &_port : flow.ports)
        fprintf(pOutFile, "<th>%s<div class=\"th_float\">%s</div></th>\n", _port.name.c_str(), _port.name.c_str());

      fputs("</tr>\n<tr>", pOutFile);

      for (size_t i = 0; i < flow.ports.size(); i++)
      {
        fputs("<td>\n", pOutFile);

        for (const auto &_inst : flow.instructionExecutionInfo)
        {
          size_t iterationIndex = (size_t)-1;

          for (const auto &_iter : _inst.perIteration)
          {
            ++iterationIndex;

            for (const auto &_port : _inst.usage)
            {
              if (_port.resourceIndex != i)
                continue;

              fprintf(pOutFile, "<div class=\"laneinst\" title=\"%s (Iteration %" PRIu64 ")\" idx=\"%" PRIu64 "\" iter=\"%" PRIu64 "\" lane=\"%" PRIu64 "\" style=\"--iter: %" PRIu64 "; --off: %" PRIu64 "; --len: %" PRIu64 "; --idx: %" PRIu64 "; --lane: %" PRIu64 ";\"></div><div class=\"instex\" idx=\"%" PRIu64 "\">\n", disassemblyLines[_inst.instructionIndex].c_str(), iterationIndex + 1, _inst.instructionIndex, iterationIndex, i, iterationIndex, _iter.clockIssued, _iter.clockExecuted - _iter.clockIssued, _inst.instructionIndex, i, _inst.instructionIndex);
              fprintf(pOutFile, "\t<div class=\"inst dispatched\" style=\"--s: %" PRIu64 "; --l: %" PRIu64 ";\"></div>\n", _iter.clockDispatched, _iter.clockPending - _iter.clockDispatched);
              fprintf(pOutFile, "\t<div class=\"inst pending\" style=\"--s: %" PRIu64 "; --l: %" PRIu64 ";\"></div>\n", _iter.clockPending, _iter.clockReady - _iter.clockPending);
              fprintf(pOutFile, "\t<div class=\"inst ready\" style=\"--s: %" PRIu64 "; --l: %" PRIu64 ";\"></div>\n", _iter.clockReady, _iter.clockIssued - _iter.clockReady);
              fprintf(pOutFile, "\t<div class=\"inst executing\" style=\"--s: %" PRIu64 "; --l: %" PRIu64 ";\"></div>\n", _iter.clockIssued, _iter.clockExecuted - _iter.clockIssued);
              fprintf(pOutFile, "\t<div class=\"inst retiring\" style=\"--s: %" PRIu64 "; --l: %" PRIu64 ";\"></div>\n", _iter.clockExecuted, _iter.clockRetired - _iter.clockExecuted);
              fputs("</div>\n", pOutFile);
            }
          }
        }

        fputs("</td>", pOutFile);
      }

      fputs("</tr>\n</table>\n</div>", pOutFile);
    }

    fputs(_HtmlAfterDocScript, pOutFile);

    fputs("</body>\n</html>", pOutFile);
    fclose(pOutFile);
  }

  return 0;
}
