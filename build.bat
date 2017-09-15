@echo off

IF NOT EXIST build mkdir build


REM  better way?
xcopy /s /y /q lib\ffmpeg-3.3.3-win64-shared\bin\*.dll build


if not defined DevEnvDir (
    call "D:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
)



set WarningFlags= -we4302

REM  -WX     (treat warnings as errors)
REM  -wd---- (disable ---- warnings)
REM  -we---- (treat ---- warnings as errors)
REM  -wd4201 (nameless struct/union)
REM  -wd4100 (unused parameters)
REM  -wd4189 (unused local var)
REM  -wd4244 (implicit cast)
REM  -wd4505 (unused local functions)
REM  -wd4389 (unsigned/sign comparisons)
REM  -we4302 (treat truncation warnings as errors)
REM  -Wv:18  (disable warnings introduced after compiler v18)



set LinkerFlags= -link -LIBPATH:"..\lib\ffmpeg-3.3.3-win64-dev\lib"

REM  -link           pass the rest of the args to the linker
REM  -LIBPATH:dir    add a lib directory (.lib)



set CompilerFlags= -FC -Zi -I"..\lib\ffmpeg-3.3.3-win64-dev\include" 

REM  -Idir           add an include directory (.h)
REM  -Zi             enable debugging info (.pdb)
REM  -FC             full paths in errors (for sublime error regex)


pushd build
cl -nologo %CompilerFlags% %WarningFlags% ..\vid.cpp %LinkerFlags% user32.lib Winmm.lib
popd


