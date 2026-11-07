@echo off
rem Builds the README preview renderer. No foobar2000 SDK needed.
rem Run from the repository root:
rem     tools\preview\build_preview.cmd && tools\preview\preview.exe
call "D:\SDK\VisualStudio\VC\Auxiliary\Build\vcvars64.bat" >nul
pushd "%~dp0"
cl /nologo /std:c++17 /EHsc /O2 /W3 /D_CRT_SECURE_NO_WARNINGS preview.cpp ^
   /link d2d1.lib dwrite.lib windowscodecs.lib ole32.lib /OUT:preview.exe
popd
