@echo off
rem Plays a mission in the VR viewer, with the game running inside it.
rem Usage: vr\run_vr.cmd campaign,mission [--desktop]
rem   e.g. vr\run_vr.cmd 0,1
rem Game data folder: %SWARS_GAMEDATA%, default ..\gamedata next to the repo.
setlocal
set REPO=%~dp0..
if "%SWARS_GAMEDATA%"=="" set SWARS_GAMEDATA=%REPO%\..\gamedata
set VIEWER=%REPO%\build\vr-viewer\syndwars-vr.exe
if not exist "%VIEWER%" (
  echo Viewer not built: run tools\decomp-agent\build.sh --x64, then vr\viewer\build.sh
  exit /b 1
)

rem cmd splits "0,1" at the comma, so the mission parts are joined back here
set MISSION=
set VIEWER_ARGS=
:args
if "%~1"=="" goto run
if "%~1"=="--desktop" (
  set VIEWER_ARGS=--desktop
) else if defined MISSION (
  call set MISSION=%%MISSION%%,%~1
) else (
  set MISSION=%~1
)
shift
goto args

:run
if not defined MISSION set MISSION=0,1
"%VIEWER%" --mission %MISSION% --data "%SWARS_GAMEDATA%" %VIEWER_ARGS%
endlocal
