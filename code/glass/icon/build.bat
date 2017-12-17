@echo off

if not defined DevEnvDir (
    call "D:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
)

REM could call from main build.bat
REM dont forget to add ../icon/resource.res to linker

rc resource.rc


