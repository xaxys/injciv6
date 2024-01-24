@echo off
setlocal
set MINGW32=%~dp0mingw32
set MINGW64=%~dp0mingw64
set PATH=%MINGW64%\bin;%MINGW32%\bin;%PATH%
echo Environment variables set. Entering bash...
cmd
endlocal
pause