@echo off

set filesToOpen=code\ghoster.cpp


if not defined DevEnvDir (
    call "D:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
)


start "" "D:\Program Files\Sublime Text 3\sublime_text.exe" %filesToOpen%

start "" "devenv" "debug\ghoster.exe" %filesToOpen%

start cmd.exe
