@echo off
rem Plays a mission in the VR viewer, with the game running inside it.
rem Usage: vr\run_vr.cmd [campaign,mission] [viewer options...]
rem   e.g. vr\run_vr.cmd 0,1
rem        vr\run_vr.cmd 1,2 --look upgraded --fog-scale 1.5
rem The options are those of docs/vr-viewer-options.md; they are passed on.
rem Game data folder: %SWARS_GAMEDATA%, default ..\gamedata next to the repo.
setlocal
set REPO=%~dp0..
if "%SWARS_GAMEDATA%"=="" set SWARS_GAMEDATA=%REPO%\..\gamedata
set VIEWER=%REPO%\build\vr-viewer\syndwars-vr.exe
if not exist "%VIEWER%" (
  echo Viewer not built: run tools\decomp-agent\build.sh --x64, then vr\viewer\build.sh
  exit /b 1
)

rem cmd splits "0,1" at the comma into 0 and 1: a leading number and the
rem one after it are the mission; everything else goes to the viewer
set MISSION=
set VIEWER_ARGS=
set FIRST=%~1
if not defined FIRST goto args
set C1=%FIRST:~0,1%
if "%C1%" GEQ "0" if "%C1%" LEQ "9" (
  set MISSION=%~1,%~2
  shift
  shift
)
:args
if "%~1"=="" goto run
set VIEWER_ARGS=%VIEWER_ARGS% %1
shift
goto args

:run
rem without a mission: the whole game, its intro and menus first
if not defined MISSION goto full
"%VIEWER%" --mission %MISSION% --data "%SWARS_GAMEDATA%" %VIEWER_ARGS%
goto done
:full
"%VIEWER%" --data "%SWARS_GAMEDATA%" %VIEWER_ARGS%
:done
endlocal
