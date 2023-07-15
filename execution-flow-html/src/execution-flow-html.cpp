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

            table.flow tr {
              line-break: anywhere;
              text-align: justify;
            }
            
            .laneinst {
                --colorA: hsl(calc(var(--lane) * 0.41 * 360deg) 50% 50%);
                --colorB: hsl(calc(var(--lane) * 0.41 * 360deg + 50deg) 30% 40%);
                content: ' ';
                width: 20pt;
                height: calc(20pt * var(--len));
                top: calc(var(--off) * 20pt + 50pt);
                background: linear-gradient(180deg, var(--colorA), var(--colorB));
                border-radius: 10pt;
                display: block;
                padding: 0;
                overflow: hidden;
                position: absolute;
                mix-blend-mode: screen;
                border: 1pt solid transparent;
                opacity: 30%;
            }

            .laneinst.selected {  
                border: 1pt solid var(--colorA);
                opacity: 100%;
                z-index: 10;
            }

            .inst_base {
                width: 100%;
                height: 100%;
            }

            .instex {
                opacity: 0%;
            }

            .instex.selected {
                opacity: 100%;
            }
            
            .inst {
              position: absolute;
              width: 4pt;
              top: calc(var(--s) * 20pt + 50pt);
              height: calc(var(--l) * 20pt);
              border-radius: 5pt;
              margin-left: 8pt;
            }

            .inst.dispatched {
              background: #ffffff20;
            }

            .inst.pending {
              background: #2d639ba6;
            }

            .inst.ready {
              background: #adf3ffd9;
            }

            .inst.executing {
              background: #fff;
            }

            .inst.retiring {
              background: #ffffff05;
            }

            .main {
              display: flex;
            }
            
            .disasmcontainer, .flowgraph {
              resize: horizontal;
            }
            
            .disasm {
              min-width: 480pt;
              cursor: default;
            }
            
            span.linenum {
              color: #444;
            }
            
            span.asm {
              color: #aaa;
            }
            
            div.disasmline.selected span.linenum {
                color: #666;
            }
            
            div.disasmline.selected span.asm {
                color: #fff;
            }
        </style>
        <div class="main">
)TEXT";

    fputs(documentSetupHtml, pOutFile);

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

        fprintf(pOutFile, "<div class=\"disasmline\" idx=\"%" PRIu64 "\"><span class=\"linenum\">0x%08" PRIX64 "&emsp;</span><span class=\"asm\">%s&emsp;</span></div>\n", instructionIndex, virtualAddress + addressDisplayOffset, disasmBuffer);

        virtualAddress += instruction.length;
      }

      fputs("</div>\n</div>\n", pOutFile);
    }

    // Add flow graph.
    {
      fputs("<div class=\"flowgraph\"><table class=\"flow\"><tr>\n", pOutFile);

      for (const auto &_port : flow.ports)
        fprintf(pOutFile, "<th>%s</th>\n", _port.name.c_str());

      fputs("</tr>\n<tr>", pOutFile);

      for (size_t i = 0; i < flow.ports.size(); i++)
      {
        fputs("<td>\n", pOutFile);

        for (const auto &_inst : flow.instructionExecutionInfo)
        {
          for (const auto &_port : _inst.usage)
          {
            if (_port.resourceIndex != i)
              continue;

            fprintf(pOutFile, "<div class=\"laneinst\" idx=\"%" PRIu64 "\" style=\"--off: %" PRIu64 "; --len: %" PRIu64 "; --idx: %" PRIu64 "; --lane: %" PRIu64 ";\"></div><div class=\"instex\" idx=\"%" PRIu64 "\">\n", _inst.instructionIndex, _inst.clockIssued, _inst.clockExecuted - _inst.clockIssued, _inst.instructionIndex, i, _inst.instructionIndex);
            fprintf(pOutFile, "\t<div class=\"inst dispatched\" style=\"--s: %" PRIu64 "; --l: %" PRIu64 ";\"></div>\n", _inst.clockDispatched, _inst.clockPending - _inst.clockDispatched);
            fprintf(pOutFile, "\t<div class=\"inst pending\" style=\"--s: %" PRIu64 "; --l: %" PRIu64 ";\"></div>\n", _inst.clockPending, _inst.clockReady - _inst.clockPending);
            fprintf(pOutFile, "\t<div class=\"inst ready\" style=\"--s: %" PRIu64 "; --l: %" PRIu64 ";\"></div>\n", _inst.clockReady, _inst.clockIssued - _inst.clockReady);
            fprintf(pOutFile, "\t<div class=\"inst executing\" style=\"--s: %" PRIu64 "; --l: %" PRIu64 ";\"></div>\n", _inst.clockIssued, _inst.clockExecuted - _inst.clockIssued);
            fprintf(pOutFile, "\t<div class=\"inst retiring\" style=\"--s: %" PRIu64 "; --l: %" PRIu64 ";\"></div>\n", _inst.clockExecuted, _inst.clockRetired - _inst.clockExecuted);

            fputs("</div>\n", pOutFile);
          }
        }

        fputs("</td>", pOutFile);
      }

      fputs("</tr>\n</table>\n</div>\n", pOutFile);
    }

    const char *script = R"SCRIPT(
<script>
var lines = document.getElementsByClassName("disasmline");
var inst0 = document.getElementsByClassName("laneinst");
var inst1 = document.getElementsByClassName("instex");

for (var i = 0; i < lines.length; i++) {
  lines[i].onmouseenter = (e) => {
    var line = e.target;

    if (line.parentElement.className.startsWith('disasmline'))
      line = line.parentElement;

    line.className = "disasmline selected";

    var idx = line.attributes['idx'].value;
    
    for (var j = 0; j < inst0.length; j++) {
      if (inst0[j].attributes['idx'].value != idx)
        continue;
      
      inst0[j].className = "laneinst selected";
    }
    
    for (var j = 0; j < inst1.length; j++) {
      if (inst1[j].attributes['idx'].value != idx)
        continue;
      
      inst1[j].className = "instex selected";
    }
  };

  lines[i].onmouseleave = (e) => {
    var line = e.target;

    if (line.parentElement.className.startsWith('disasmline'))
      line = line.parentElement;

    line.className = "disasmline";

    var idx = line.attributes['idx'].value;
    
    for (var j = 0; j < inst0.length; j++) {
      if (inst0[j].attributes['idx'].value != idx)
        continue;
      
      inst0[j].className = "laneinst";
    }
    
    for (var j = 0; j < inst1.length; j++) {
      if (inst1[j].attributes['idx'].value != idx)
        continue;
      
      inst1[j].className = "instex";
    }
  };
}
</script>
)SCRIPT";

    fputs(script, pOutFile);

    fputs("</div>\n</body>\n</html>", pOutFile);
    fclose(pOutFile);
  }

  return 0;
}
