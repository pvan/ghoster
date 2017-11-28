@echo off

REM note %~dp0 seems to include a trailing \ already
set PATH=%~dp0build
set EXE=ghoster


REM delete any old copies of the exe
del %PATH%\%EXE%_*.exe >NUL 2>NUL

REM get a "unique" string to name the new copy
set rand=%random%

REM make a copy of the exe
copy %PATH%\%EXE%.exe %PATH%\%EXE%_%rand%.exe

REM start the copy
start /d %PATH% %EXE%_%rand%.exe

