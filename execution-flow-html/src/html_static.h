#ifndef html_constants_h__
#define html_constants_h__


const char *_HtmlDocumentSetup = R"TEXT(<!DOCTYPE html>
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
            
            ::-webkit-scrollbar {
                width: 5pt;
                background: #3a3a3a;
            }
            
            ::-webkit-scrollbar-track {
                background: rgba(1, 1, 1, 0);
            }
            
            ::-webkit-scrollbar-thumb {
                background: #a0a0a0;
            }
            
            table.flow tr {
              line-break: anywhere;
              text-align: justify;
            }

            th {
              color: #fff0;
            }

            .th_float {
              position: fixed;
              top: 8pt;
              padding: 2pt 5pt;
              border-radius: 2pt;
              margin-left: -5pt;
              color: #fff;
            }

            th, .th_float {
              width: calc((100% - 480pt) / var(--lane-count) - 5pt);
            }
            
            .laneinst {
                --colorA: hsl(calc(var(--lane) * 0.41 * 360deg - var(--iter) * 0.2 * 360deg) 50% 50%);
                --colorB: hsl(calc(var(--lane) * 0.41 * 360deg + 50deg - var(--iter) * 0.2 * 360deg) 30% 40%);
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

            .laneinst.dependent {
              opacity: 100;
              --colorA: hsl(calc(var(--lane) * 0.41 * 360deg - var(--iter) * 0.2 * 360deg) 70% 60% / 0.3);
              --colorB: hsl(calc(var(--lane) * 0.41 * 360deg + 50deg - var(--iter) * 0.2 * 360deg) 40% 50% / 0.1);
              --colorC: hsl(calc(var(--lane) * 0.41 * 360deg - var(--iter) * 0.2 * 360deg) 70% 50%);
              border: 2pt dotted var(--colorC);
            }
            
            .laneinst.dependent::after {
              content: var(--type);
              font-size: 80%;
              text-transform: uppercase;
              padding: 0pt 2pt;
              text-shadow: 0 0 var(--colorC);
              color: #fff7;
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
              font-size: 12pt;
              line-height: 120%;
            }
            
            span.linenum {
              color: #444;
            }
            
            span.asm {
              --exclim: calc(min(var(--exec), 20) / 20);
              --exc2: calc(1 - (1 - var(--exclim)) * (1 - var(--exclim)));
              color: hsl(calc(125deg - var(--exc2) * 80deg), calc(var(--exc2) * 100%), calc(40% + var(--exc2) * 25%));
            }

            span.linenum.highlighted {
              color: #998c66;
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
              color: #c6b584;
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

            .stall {
              color: #ff7171;
              font-size: 82%;
              display: block;
              margin: 3pt 0;
            }

            .registers {
              color: #ccc;
              font-size: 82%;
              display: block;
              margin: 5pt 0;
            }

            span.rsrc {
              --colorA: hsl(calc(var(--lane) * 0.41 * 360deg) 90% 80%);
              --colorB: hsl(calc(var(--lane) * 0.41 * 360deg) 30% 30%);
              --colorC: hsl(calc(var(--lane) * 0.41 * 360deg + 40deg) 40% 40%);
              border: solid 1pt var(--colorA);
              color: #fff;
              padding: 2pt 8pt;
              background: linear-gradient(180deg, var(--colorB), var(--colorC));
              border-radius: 10pt;
              display: inline-block;
              margin: 4pt 4pt 2pt 0pt;
            }

            div.spacer {
              width: 1pt;
              height: 1pt;
              margin-bottom: 500pt;
            }

            .dependency {
              font-size: 85%;
            }
            
            .dependency.register {
              color: #b18597;
            }
            
            .dependency.memory {
              color: #85b1a7;
            }
            
            .dependency.resource {
              color: #aa9fdf;
            }

            span.press_obj {
              display: inline-block;
              background: #1c1c1c;
              padding: 0 3.5pt;
              border-radius: 5pt;
              color: #ececec;
            }

            span.loop, span.loop_origin {
              display: inline-block;
              color: #fff;
              background: #4924ff;
              border-radius: 5pt;
              padding: 0 3pt;
              box-shadow: 1pt 1pt #000;
            }

            span.loop_origin {
              background: #6c629b;
            }

            span.loop::before {
              content: 'Loop ';
              font-size: 70%;
            }

            span.loop_origin::before {
              content: '◀ Loop ';
              font-size: 70%;
            }
            
            div.disasmline.selected div.depptr, div.disasmline.selected div.depptr::before {
              --lineheight: 14.4pt;
              position: absolute;
              width: 10pt;
              height: calc(var(--e) * var(--lineheight));
              background: transparent;
              border: 2pt solid #ff0b7d;
              border-right: none;
              margin-top: calc(-7.5pt - var(--lineheight) * var(--e));
              margin-left: 60pt;
              border-radius: 5pt 0 0 5pt;
            }

            div.disasmline.selected div.depptr::before {
              content: ' ';
              margin: -1.5pt;
              margin-top: calc(-1.6pt + var(--lineheight) * var(--e));
              height: calc((0 - var(--e)) * var(--lineheight) - 1.5pt);
              opacity: calc(var(--e) > 0 ? 1: 0);
            }
            
            div.disasmline.selected div.depptr::after {
              content: ' ';
              margin: -4pt 0 0 3.5pt;
              width: 5pt;
              height: 5pt;
              border-radius: 0;
              border: 1.5pt solid #ff0b7d;
              border-bottom: none;
              border-left: none;
              transform: rotate(45deg);
              position: absolute;
              background: transparent;
            }

            div.disasmline.selected div.depptr.resource {
              width: 20pt;
              left: -10pt;
            }

            div.disasmline.selected div.depptr.resource::before {
              width: 20pt;
            }
            
            div.disasmline.selected div.depptr.resource::after {
              margin-left: 14pt;
            }
            
            div.disasmline.selected div.depptr.register, div.disasmline.selected div.depptr.register::before, div.disasmline.selected div.depptr.register::after {
              border-color: #ff0b7d;
            }
            
            div.disasmline.selected div.depptr.memory, div.disasmline.selected div.depptr.memory::before, div.disasmline.selected div.depptr.memory::after {
              border-color: #0bffe8;
            }
            
            div.disasmline.selected div.depptr.resource, div.disasmline.selected div.depptr.resource::before, div.disasmline.selected div.depptr.resource::after {
              border-color: #8372f9;
            }

            .disasmline.selected.static {
              background: #aaa;
              cursor: pointer;
              user-select: none;
            }

            .disasmline.selected.static span.linenum, .disasmline.selected.static span.asm {
              filter: invert();
            }

            .stats {
              margin: 5em 0.5em;
              border-left: solid 3pt #35949e;
              padding-left: 0.75em;
            }

            .stats_it h2 {
              font-size: 14pt;
              color: #a7e4ea;
              margin-bottom: 5pt;
              margin-top: 20pt;
            }
            
            .stats_it b {
              display: block;
              margin-bottom: 1pt;
            }

            .stats_it i {
              display: block;
              color: #7cb9c0;
            }

            .stats_it i i {
              display: inline;
              font-size: 90%;
              opacity: 50%;
            }

            .stats_it i.s {
              color: hsl(calc(var(--h) * -150deg + 180deg), calc(var(--h) * 60% + 20%), calc(var(--h) * 20% + 40%));
              font-size: 89%;
              line-height: 110%;
            }
        </style>
        <div class="main">
)TEXT";

const char *_HtmlAfterDocScript = R"SCRIPT(
</div>
<script>
var lines = document.getElementsByClassName("disasmline");
var inst0 = document.getElementsByClassName("laneinst");
var inst1 = document.getElementsByClassName("instex");

var clicked = null;

for (var i = 0; i < lines.length; i++) {
  lines[i].onmouseenter = (e) => {
    if (clicked != null)
      return;
    
    var line = e.target;

    if (line.parentElement.className.startsWith('disasmline'))
      line = line.parentElement;

    var chld = line.children;
    var selc = null;
    
    for (var i = 0; i < chld.length; i++)
      if (chld[i].className == 'dependency_data')
        selc = chld[i];
    
    if (selc != null) {
      chld = selc.children;
      
      for (var i = 0; i < chld.length; i++) {
        var x = chld[i];
        
        for (var j = 0; j < inst0.length; j++) { 
          var n = inst0[j];

          if (x.attributes['iteration'].value == n.attributes['iter'].value && x.attributes['index'].value == n.attributes['idx'].value) {
            n.className = "laneinst dependent";
            n.style.setProperty('--cycles', x.attributes['cycles'].value);
            
            if (x.className == '__reg')
              n.style.setProperty('--type', "'reg'");
            else if (x.className == '__mem')
              n.style.setProperty('--type', "'mem'");
            else if (x.className == '__rsc')
              n.style.setProperty('--type', "'rsc'");
            else
              n.style.setProperty('--type', "'??'");
          }
        }
      }
    }

    line.className = "disasmline selected";

    var idx = line.attributes['idx'].value;
    
    for (var j = 0; j < inst0.length; j++) {
      if (inst0[j].attributes['idx'].value != idx)
        continue;
      
      inst0[j].className += " selected";
    }
    
    for (var j = 0; j < inst1.length; j++) {
      if (inst1[j].attributes['idx'].value != idx)
        continue;
      
      inst1[j].className = "instex selected";
    }
  };

    lines[i].onclick = (e) => {
      var line = e.target;

      if (line.parentElement.className.startsWith('disasmline'))
        line = line.parentElement;
      
      if (clicked != line) {
        if (clicked != null) {
          var old = clicked;
          clicked = null;
          old.onmouseleave({ target: old });
          line.onmouseenter({ target: line });
        } else {
          clicked = line;
          clicked.className = "disasmline selected static";
        }
      } else {
        clicked.className = "disasmline selected";
        clicked = null;
      }
    }

  lines[i].onmouseleave = (e) => {
    if (clicked != null)
      return;
    
    var line = e.target;

    if (line.parentElement.className.startsWith('disasmline'))
      line = line.parentElement;
    
    var chld = line.children;
    var selc = null;
    
    for (var i = 0; i < chld.length; i++)
      if (chld[i].className == 'dependency_data')
        selc = chld[i];
    
    if (selc != null) {
      chld = selc.children;
      
      for (var i = 0; i < chld.length; i++) {
        var x = chld[i];
        
        for (var j = 0; j < inst0.length; j++) { 
          var n = inst0[j];

          if (x.attributes['iteration'].value == n.attributes['iter'].value && x.attributes['index'].value == n.attributes['idx'].value)
            n.className = "laneinst";
        }
      }
    }

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

#endif // html_constants_h__
