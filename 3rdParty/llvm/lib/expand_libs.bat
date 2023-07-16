IF EXIST 3rdParty/llvm/lib/.cookie (GOTO END)

powershell -command Expand-Archive "3rdParty/llvm/lib/llvm_libs.zip" "3rdParty/llvm/lib" -Force
IF %ERRORLEVEL% NEQ 0 (exit 1)

powershell -command Expand-Archive "3rdParty/llvm/lib/llvm_libs0.zip" "3rdParty/llvm/lib" -Force
IF %ERRORLEVEL% NEQ 0 (exit 1)

powershell -command Expand-Archive "3rdParty/llvm/lib/llvm_libs1.zip" "3rdParty/llvm/lib" -Force
IF %ERRORLEVEL% NEQ 0 (exit 1)

powershell -command Expand-Archive "3rdParty/llvm/lib/llvm_libs2.zip" "3rdParty/llvm/lib" -Force
IF %ERRORLEVEL% NEQ 0 (exit 1)

echo "" > 3rdParty/llvm/lib/.cookie

:END