@ECHO OFF

IF [%1]==[] GOTO MANUAL_CONFIG

IF "%1"=="1" GOTO ONE;
IF "%1"=="2" GOTO TWO;
IF "%1"=="3" GOTO THREE;
IF "%1"=="4" GOTO FOUR;

ECHO INVALID PARAMETER (%1)

:MANUAL_CONFIG
ECHO 1. Visual Studio 2022 Solution
ECHO 2. Visual Studio 2019 Solution
ECHO 3. Visual Studio 2017 Solution
ECHO 4. Visual Studio 2015 Solution

CHOICE /N /C:1234 /M "[1-4]:"

IF ERRORLEVEL ==4 GOTO FOUR
IF ERRORLEVEL ==3 GOTO THREE
IF ERRORLEVEL ==2 GOTO TWO
IF ERRORLEVEL ==1 GOTO ONE
GOTO END

:FOUR
 ECHO Creating VS2015 Project...
 premake\premake5.exe vs2015
 GOTO END

:THREE
 ECHO Creating VS2017 Project...
 premake\premake5.exe vs2017
 GOTO END

:TWO
 ECHO Creating VS2019 Project...
 premake\premake5.exe vs2019
 GOTO END

:ONE
 ECHO Creating VS2022 Project...
 premake\premake5.exe vs2022
 GOTO END

:END
3rdParty/llvm/lib/expand_libs.bat
