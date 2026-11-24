@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "%~dp0MouseFix"
msbuild MouseFix.sln /m /p:Configuration=Release /p:Platform=x64 /v:minimal
if errorlevel 1 exit /b 1
msbuild MouseFix.sln /m /p:Configuration=Release /p:Platform=x86 /v:minimal
