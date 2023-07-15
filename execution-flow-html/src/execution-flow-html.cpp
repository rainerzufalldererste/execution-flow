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

////////////////////////////////////////////////////////////////////////////////

#pragma optimize ("", off)

int main(int argc, char **pArgv)
{
  if (argc < 3)
  {
    puts("Usage: execution-flow-html <RawAssembledBinaryFile> <AnalysisFile.html>");
    return 0;
  }

  const char *inFilename = pArgv[1];
  const char *outFilename = pArgv[2];

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
  const bool result = execution_flow_create(pData, fileSize, &flow);

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

    const char *documentSetupHtml = R"TEXT(<!DOCTYPE html>
<html>
    <head>
        <meta charset="utf-8">
        <title>Flow Analysis</title>
    </head>
    <body>
        <style>
            body {
                background: #111;
                color: #fff;
                font-family: Consolas, monospace;
            }

            table.flow {
                width: 100%;
            }
            
            .laneinst {
                --colorA: hsl(calc(var(--idx) * 24.5deg) 50% 50%);
                --colorB: hsl(calc(var(--idx) * 24.5deg + 5.2deg) 50% 70%);
                content: ' ';
                width: 10pt;
                height: calc(20pt * var(--len));
                margin-top: calc(var(--off) * 20pt);
                background: linear-gradient(45deg, var(--colorA), var(--colorB));
                border-radius: 62pt;
                display: inline-block;
                padding: 0;
                overflow: hidden;
                border: 1pt solid var(--colorA);
                position: absolute;
                mix-blend-mode: screen;
                margin-left: calc(var(--lane) * 12pt);
                float: left;
            }

            .inst_base {
                width: 100%;
                height: 100%;
            }

            .inst {
                margin-top: 0;
                width: 100%;
                height: calc(20pt * var(--l));
                opacity: 100%;
                background-color: #000;
            }

            .inst.dispatched {
                margin-top: calc(20pt * var(--s));
            }

            .inst.pending {
              /* empty */
            }

            .inst.ready {
                opacity: 85%;
            }

            .inst.executing {
                opacity: 0%;
            }

            .inst.retiring {
              /* empty */
            }
        </style>
        <table class="flow">
            <tr>
)TEXT";

    fputs(documentSetupHtml, pOutFile);

    for (const auto &_port : flow.ports)
      fprintf(pOutFile, "<th>%s</th>\n", _port.name.c_str());

    fputs("</tr>\n<tr>", pOutFile);

    for (size_t i = 0; i < flow.ports.size(); i++)
    {
      fputs("<td>\n", pOutFile);

      std::vector<std::vector<bool>> subLanes;

      for (const auto &_inst : flow.instructionExecutionInfo)
      {
        for (const auto &_port : _inst.usage)
        {
          if (_port.resourceIndex != i)
            continue;

          const size_t start = std::min(_inst.clockIssued, _inst.clockIssued - 1);
          const size_t end = _inst.clockExecuted + 1;

          size_t lane = (size_t)-1;

          for (size_t l = 0; l < subLanes.size(); l++)
          {
            if (subLanes[l].size() <= start)
            {
              lane = l;
              break;
            }
            else
            {
              bool full = false;

              const size_t target = std::min(subLanes[l].size(), end + 1);

              for (size_t j = start; j < target; j++)
                full |= subLanes[l][j];

              if (!full)
              {
                lane = l;
                break;
              }
            }
          }

          if (lane == (size_t)-1)
          {
            lane = subLanes.size();
            subLanes.emplace_back();
          }

          // Fill Space.
          {
            subLanes[lane].reserve(end + 1);

            while (subLanes[lane].size() < end)
              subLanes[lane].emplace_back(false);

            for (size_t j = start; j < end; j++)
              subLanes[lane][j] = true;
          }

          fprintf(pOutFile, "<div class=\"laneinst\" style=\"--off: %" PRIu64 "; --len: %" PRIu64 "; --idx: %" PRIu64 "; --lane: %" PRIu64 ";\"><div class=\"inst_base\">\n", _inst.clockIssued, _inst.clockExecuted - _inst.clockIssued, _inst.instructionIndex, lane);
          fprintf(pOutFile, "\t<div class=\"inst dispatched\" style=\"--s: %" PRIi64 "; --l: %" PRIu64 ";\"></div>\n", (int64_t)_inst.clockDispatched - _inst.clockIssued, _inst.clockReady - _inst.clockDispatched);
          fprintf(pOutFile, "\t<div class=\"inst pending\" style=\"execution--l: %" PRIu64 ";\"></div>\n", _inst.clockIssued - _inst.clockPending);
          fprintf(pOutFile, "\t<div class=\"inst ready\" style=\"execution--l: %" PRIu64 ";\"></div>\n", _inst.clockIssued - _inst.clockReady);
          fprintf(pOutFile, "\t<div class=\"inst executing\" style=\"execution--l: %" PRIu64 ";\"></div>\n", _inst.clockExecuted - _inst.clockIssued);
          fprintf(pOutFile, "\t<div class=\"inst retiring\" style=\"execution--l: %" PRIu64 ";\"></div>\n", _inst.clockRetired - _inst.clockExecuted);

          fputs("</div></div>\n", pOutFile);
        }
      }

      fputs("</td>", pOutFile);
    }

    fputs("</tr>\n</table>\n</body>\n</html>", pOutFile);

    fclose(pOutFile);
  }

  return 0;
}
