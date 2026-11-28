@echo off
rem Plays Syndicate Wars with the upgraded look available (F7 switches).
rem Usage: vr\run_hd.cmd [campaign,mission] [options...]
rem   e.g. vr\run_hd.cmd             the whole game, intro and menus first
rem        vr\run_hd.cmd 0,1 --look upgraded
rem The options are those of docs/vr-viewer-options.md ("The desktop window").
rem Game data folder: %SWARS_GAMEDATA%, default ..\gamedata next to the repo.
setlocal
set REPO=%~dp0..
if "%SWARS_GAMEDATA%"=="" set SWARS_GAMEDATA=%REPO%\..\gamedata
set HD=%REPO%\build\vr-viewer\syndwars-hd.exe
if not exist "%HD%" (
  echo Not built: run tools\decomp-agent\build.sh --x64, then vr\viewer\build.sh
  exit /b 1
)

rem cmd splits "0,1" at the comma into 0 and 1: a leading number and the
rem one after it are the mission; everything else goes to the program
set MISSION=
set ARGS=
set FIRST=%~1
if not defined FIRST goto args
set C1=%FIRST:~0,1%
if "%C1%" GEQ "0" if "%C1%" LEQ "9" (
  set MISSION=--mission %~1,%~2
  shift
  shift
)
:args
if "%~1"=="" goto run
set ARGS=%ARGS% %1
shift
goto args

:run
"%HD%" %MISSION% --data "%SWARS_GAMEDATA%" %ARGS%
endlocal
