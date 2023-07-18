ProjectName = "execution-flow"
project(ProjectName)

  --Settings
  kind "StaticLib"
  language "C++"
  staticruntime "On"

  cppdialect "C++17"

  filter { "system:windows" }
    buildoptions { '/Gm-' }
    buildoptions { '/MP' }

    ignoredefaultlibraries { "msvcrt" }
  filter { "system:linux" }

  filter { }
  
  filter { "configurations:Release" }
    flags { "LinkTimeOptimization" }
  
  filter { }
  
  defines { "_CRT_SECURE_NO_WARNINGS", "SSE2" }
  
  objdir "intermediate/obj"

  files { "src/**.cpp", "src/**.c", "src/**.cc", "src/**.h", "src/**.hh", "src/**.hpp", "src/**.inl", "src/**rc" }
  files { "include/**.h", "include/**.hh", "include/**.hpp", "include/**.inl" }
  files { "project.lua" }
  
  includedirs { "include", "include/**", "src", "src/**" }
  includedirs { "../3rdParty/llvm/include" }

  filter { "configurations:Debug", "system:Windows" }
    ignoredefaultlibraries { "libcmt" }
  filter { }

  filter { "configurations:Release", "system:Windows" }
    linkoptions { "../3rdParty/llvm/lib/LLVMAnalysis.lib", "../3rdParty/llvm/lib/LLVMBitstreamReader.lib", "../3rdParty/llvm/lib/LLVMCodeGen.lib", "../3rdParty/llvm/lib/LLVMTransformUtils.lib", "../3rdParty/llvm/lib/LLVMTarget.lib", "../3rdParty/llvm/lib/LLVMSelectionDAG.lib", "../3rdParty/llvm/lib/LLVMGlobalISel.lib", "../3rdParty/llvm/lib/LLVMCFGuard.lib", "../3rdParty/llvm/lib/LLVMScalarOpts.lib", "../3rdParty/llvm/lib/LLVMObjCARCOpts.lib", "../3rdParty/llvm/lib/LLVMDebugInfoCodeView.lib", "../3rdParty/llvm/lib/LLVMDebugInfoPDB.lib", "../3rdParty/llvm/lib/LLVMMC.lib", "../3rdParty/llvm/lib/LLVMMCParser.lib", "../3rdParty/llvm/lib/LLVMAsmParser.lib", "../3rdParty/llvm/lib/LLVMRemarks.lib", "../3rdParty/llvm/lib/LLVMTargetParser.lib", "../3rdParty/llvm/lib/LLVMX86Disassembler.lib", "../3rdParty/llvm/lib/LLVMBinaryFormat.lib", "../3rdParty/llvm/lib/LLVMCodeGenTypes.lib", "../3rdParty/llvm/lib/LLVMDebugInfoDWARF.lib", "../3rdParty/llvm/lib/LLVMDemangle.lib", "../3rdParty/llvm/lib/LLVMMCA.lib", "../3rdParty/llvm/lib/LLVMObject.lib", "../3rdParty/llvm/lib/LLVMSupport.lib", "../3rdParty/llvm/lib/LLVMTextAPI.lib", "../3rdParty/llvm/lib/LLVMX86Info.lib", "../3rdParty/llvm/lib/LLVMBitReader.lib", "../3rdParty/llvm/lib/LLVMCore.lib", "../3rdParty/llvm/lib/LLVMDebugInfoMSF.lib", "../3rdParty/llvm/lib/LLVMIRReader.lib", "../3rdParty/llvm/lib/LLVMMCDisassembler.lib", "../3rdParty/llvm/lib/LLVMProfileData.lib", "../3rdParty/llvm/lib/LLVMSymbolize.lib", "../3rdParty/llvm/lib/LLVMX86CodeGen.lib", "../3rdParty/llvm/lib/LLVMX86Desc.lib", "../3rdParty/llvm/lib/LLVMX86TargetMCA.lib" }
  filter { "configurations:Debug", "system:Windows" }
    linkoptions { "../3rdParty/llvm/lib/Debug/LLVMAnalysis.lib", "../3rdParty/llvm/lib/Debug/LLVMBitstreamReader.lib", "../3rdParty/llvm/lib/Debug/LLVMCodeGen.lib", "../3rdParty/llvm/lib/Debug/LLVMTransformUtils.lib", "../3rdParty/llvm/lib/Debug/LLVMTarget.lib", "../3rdParty/llvm/lib/Debug/LLVMSelectionDAG.lib", "../3rdParty/llvm/lib/Debug/LLVMGlobalISel.lib", "../3rdParty/llvm/lib/Debug/LLVMCFGuard.lib", "../3rdParty/llvm/lib/Debug/LLVMScalarOpts.lib", "../3rdParty/llvm/lib/Debug/LLVMObjCARCOpts.lib", "../3rdParty/llvm/lib/Debug/LLVMDebugInfoCodeView.lib", "../3rdParty/llvm/lib/Debug/LLVMDebugInfoPDB.lib", "../3rdParty/llvm/lib/Debug/LLVMMC.lib", "../3rdParty/llvm/lib/Debug/LLVMMCParser.lib", "../3rdParty/llvm/lib/Debug/LLVMAsmParser.lib", "../3rdParty/llvm/lib/Debug/LLVMRemarks.lib", "../3rdParty/llvm/lib/Debug/LLVMTargetParser.lib", "../3rdParty/llvm/lib/Debug/LLVMX86Disassembler.lib", "../3rdParty/llvm/lib/Debug/LLVMBinaryFormat.lib", "../3rdParty/llvm/lib/Debug/LLVMCodeGenTypes.lib", "../3rdParty/llvm/lib/Debug/LLVMDebugInfoDWARF.lib", "../3rdParty/llvm/lib/Debug/LLVMDemangle.lib", "../3rdParty/llvm/lib/Debug/LLVMMCA.lib", "../3rdParty/llvm/lib/Debug/LLVMObject.lib", "../3rdParty/llvm/lib/Debug/LLVMSupport.lib", "../3rdParty/llvm/lib/Debug/LLVMTextAPI.lib", "../3rdParty/llvm/lib/Debug/LLVMX86Info.lib", "../3rdParty/llvm/lib/Debug/LLVMBitReader.lib", "../3rdParty/llvm/lib/Debug/LLVMCore.lib", "../3rdParty/llvm/lib/Debug/LLVMDebugInfoMSF.lib", "../3rdParty/llvm/lib/Debug/LLVMIRReader.lib", "../3rdParty/llvm/lib/Debug/LLVMMCDisassembler.lib", "../3rdParty/llvm/lib/Debug/LLVMProfileData.lib", "../3rdParty/llvm/lib/Debug/LLVMSymbolize.lib", "../3rdParty/llvm/lib/Debug/LLVMX86CodeGen.lib", "../3rdParty/llvm/lib/Debug/LLVMX86Desc.lib", "../3rdParty/llvm/lib/Debug/LLVMX86TargetMCA.lib" }
  filter { }

  targetname(ProjectName)
  targetdir "../builds/lib"
  debugdir "../builds/lib"
  
filter {}
configuration {}

warnings "Extra"

filter {}
configuration {}
flags { "NoMinimalRebuild", "NoPCH" }
exceptionhandling "Off"
rtti "Off"
floatingpoint "Fast"

filter { "configurations:Debug*" }
	defines { "_DEBUG" }
	optimize "Off"
	symbols "On"

filter { "configurations:Release" }
	defines { "NDEBUG" }
	optimize "Speed"
	flags { "NoBufferSecurityCheck", "NoIncrementalLink" }
  omitframepointer "On"
	symbols "On"

filter { "system:windows", "configurations:Release", "action:vs2012" }
	buildoptions { "/d2Zi+" }

filter { "system:windows", "configurations:Release", "action:vs2013" }
	buildoptions { "/Zo" }

filter { "system:windows", "configurations:Release" }
	flags { "NoIncrementalLink" }

editandcontinue "Off"
