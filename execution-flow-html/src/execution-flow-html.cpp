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
  const bool result = execution_flow_create(pData, fileSize, &flow, CoreArchitecture::_CurrentCPU, 8, 0);

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
                overflow-x: hidden;
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
                display: none;
                opacity: 0%;
            }

            .instex.selected {
                display: block;
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
              background: #ffffff2e;
            }

            .inst.dispatched::before {
              content: '◀ dispatched';
              position: absolute;
              width: 200pt;
              margin-left: 10pt;
              opacity: calc(min(1, var(--l)));
              margin-top: -8pt;
            }

            .inst.pending {
              background: #3c8adbad;
            }

            .inst.pending::before {
              content: '◀ pending';
              position: absolute;
              width: 200pt;
              margin-left: 10pt;
              opacity: calc(min(1, var(--l)));
              margin-top: -8pt;
            }

            .inst.ready {
              background: #66ffaaa3;
            }

            .inst.ready::before {
              content: '◀ ready';
              position: absolute;
              width: 200pt;
              margin-left: 10pt;
              opacity: calc(min(1, var(--l)));
              margin-top: -8pt;
            }

            .inst.executing {
              background: #fff;
            }

            .inst.executing::before {
              content: '◀ issued';
              position: absolute;
              width: 200pt;
              margin-left: 10pt;
              opacity: calc(min(1, var(--l)));
              margin-top: -8pt;
            }

            .inst.retiring {
              background: #ffffff26;
            }

            .inst.retiring::before {
              content: '◀ executed';
              position: absolute;
              width: 200pt;
              margin-left: 10pt;
              opacity: calc(min(1, var(--l)));
              margin-top: -8pt;
            }

            .inst.retiring::after {
              content: '◀ retired';
              position: absolute;
              width: 200pt;
              margin-left: 10pt;
              opacity: calc(min(1, var(--l)));
              margin-top: calc(var(--l) * 20pt - 8pt);
            }

            .main {
              display: block;
            }
            
            .flowgraph {
              margin-left: 480pt;
            }
            
            .disasm {
              width: 480pt;
              cursor: default;
              position: fixed;
              max-height: 100%;
              overflow-y: scroll;
            }
            
            span.linenum {
              color: #444;
            }
            
            span.asm {
              color: #aaa;
            }

            span.linenum.highlighted {
              color: #996b66;
            }

            span.asm.highlighted {
              color: #ffcaca;
            }

            span.asm.null {
              color: #555;
            }
            
            div.disasmline.selected span.linenum {
                color: #666;
            }
            
            div.disasmline.selected span.asm {
                color: #fff;
            }

            div.disasmline.selected span.linenum.highlighted {
              color: #d96054;
            }

            div.disasmline.selected span.asm.highlighted {
              color: #fff;
            }

            div.extra_info {
              position: absolute;
              left: 250pt;
              background: #272727;
              padding: 3pt 5pt;
              display: none;
              max-width: 220pt;
            }

            div.disasmline.selected div.extra_info {
              display: block;
              opacity: 80%;
            }

            .bottleneck {
              color: #ff7171;
            }

            .registers {
              color: #959595;
            }

            span.rsrc {
                --colorA: hsl(calc(var(--lane) * 0.41 * 360deg) 50% 50%);
                --colorB: hsl(calc(var(--lane) * 0.41 * 360deg) 80% 80%);
                --colorC: hsl(calc(var(--lane) * 0.41 * 360deg) 20% 20%);
                border: solid 1.5pt var(--colorA);
                color: var(--colorB);
                padding: 2pt 5pt;
                background: var(--colorC);
                border-radius: 10pt;
                display: inline-block;
                margin: 4pt 4pt 1pt 0pt;
            }

            div.spacer {
              width: 1pt;
              height: 1pt;
              margin-bottom: 500pt;
            }
        </style>
        <div class="main">
)TEXT";

    fputs(documentSetupHtml, pOutFile);

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

        const char *subVariant = instructionInfo.bottleneckInfo.size() > 0 ? " highlighted" : (instructionInfo.usage.size() == 0 && instructionInfo.clockExecuted - instructionInfo.clockIssued == 0 ? " null" : "");

        fprintf(pOutFile, "<div class=\"disasmline\" idx=\"%" PRIu64 "\"><span class=\"linenum%s\">0x%08" PRIX64 "&emsp;</span><span class=\"asm%s\">%s&emsp;</span><div class=\"extra_info\">", instructionIndex, subVariant, virtualAddress + addressDisplayOffset, subVariant, disasmBuffer);

        fprintf(pOutFile, "<div class=\"uops\">%" PRIu64 " uOps</div>", instructionInfo.uOpCount);
        fprintf(pOutFile, "<div class=\"cycleInfo\">dispatched: %" PRIu64 " cycles</div>", instructionInfo.clockPending - instructionInfo.clockDispatched);
        fprintf(pOutFile, "<div class=\"cycleInfo\">pending: %" PRIu64 " cycles</div>", instructionInfo.clockReady - instructionInfo.clockPending);
        fprintf(pOutFile, "<div class=\"cycleInfo\">ready: %" PRIu64 " cycles</div>", instructionInfo.clockIssued - instructionInfo.clockReady);
        fprintf(pOutFile, "<div class=\"cycleInfo\">executing: %" PRIu64 " cycles</div>", instructionInfo.clockExecuted - instructionInfo.clockIssued);
        fprintf(pOutFile, "<div class=\"cycleInfo\">retiring: %" PRIu64 " cycles</div>", instructionInfo.clockRetired - instructionInfo.clockExecuted);

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

        for (const auto &_b : instructionInfo.bottleneckInfo)
          fprintf(pOutFile, "<div class=\"bottleneck\">%s</div>", _b.c_str());

        fputs("</div></div>\n", pOutFile);

        virtualAddress += instruction.length;
      }

      fputs("<div class=\"spacer\"></div></div>\n</div>\n", pOutFile);
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
          for (const auto &_iter : _inst.perIteration)
          {
            for (const auto &_port : _inst.usage)
            {
              if (_port.resourceIndex != i)
                continue;

              fprintf(pOutFile, "<div class=\"laneinst\" title=\"%s\" idx=\"%" PRIu64 "\" style=\"--off: %" PRIu64 "; --len: %" PRIu64 "; --idx: %" PRIu64 "; --lane: %" PRIu64 ";\"></div><div class=\"instex\" idx=\"%" PRIu64 "\">\n", disassemblyLines[_inst.instructionIndex].c_str(), _inst.instructionIndex, _iter.clockIssued, _iter.clockExecuted - _iter.clockIssued, _inst.instructionIndex, i, _inst.instructionIndex);
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

    const char *script = R"SCRIPT(
</div>
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

    fputs("</body>\n</html>", pOutFile);
    fclose(pOutFile);
  }

  return 0;
}
