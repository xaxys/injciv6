@echo off
setlocal
set MINGW32=%~dp0mingw32
set MINGW64=%~dp0mingw64
set PATH=%PATH%;%MINGW32%\bin;%MINGW64%\bin
echo Environment variables set. Entering bash...
cmd
endlocal
pause