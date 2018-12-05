@echo off

set root=%~dp0..

if exist "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" (
  call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x64
)

set comp_flags=-MT -nologo -Gm- -GR- -EHa- -Od -Oi -FC -Z7 -TC

if not exist %root%\build mkdir %root%\build

pushd %root%\build

cl %comp_flags% %root%\src\example.c -Fecentar.exe -DWIN32_LEAN_AND_MEAN

popd
