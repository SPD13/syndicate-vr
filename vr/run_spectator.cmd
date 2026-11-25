@echo off
rem Starts Syndicate Wars with the VR spectator export, and the VR viewer.
rem Usage: vr\run_spectator.cmd [campaign,mission] [--desktop]
rem   e.g. vr\run_spectator.cmd 0,1      jump straight into a mission
rem        vr\run_spectator.cmd          normal start, menus first
rem Game data folder: %SWARS_GAMEDATA%, default ..\gamedata next to the repo.
setlocal
set REPO=%~dp0..
if "%SWARS_GAMEDATA%"=="" set SWARS_GAMEDATA=%REPO%\..\gamedata
set GAME=%REPO%\build\release\src\syndwarsfx.exe
set VIEWER=%REPO%\build\vr-viewer\syndwars-vr.exe

if not exist "%GAME%" (
  echo Game not built: run tools\decomp-agent\build.sh first
  exit /b 1
)
if not exist "%VIEWER%" (
  echo Viewer not built: run vr\viewer\build.sh first
  exit /b 1
)
rem The game needs its DLLs next to the exe
if not exist "%REPO%\build\release\src\SDL2.dll" copy /y "%SWARS_GAMEDATA%\*.dll" "%REPO%\build\release\src\" >nul

rem cmd splits "0,1" at the comma, so the mission parts are joined back here
set MISSION=
set VIEWER_ARGS=--voxel
:args
if "%~1"=="" goto run
if "%~1"=="--desktop" (
  set VIEWER_ARGS=--voxel --desktop
) else if defined MISSION (
  call set MISSION=%%MISSION%%,%~1
) else (
  set MISSION=%~1
)
shift
goto args

:run
set MISSION_ARGS=
if defined MISSION set MISSION_ARGS=-m %MISSION%
set SWARS_VR_SPECTATOR=1
pushd "%SWARS_GAMEDATA%"
start "Syndicate Wars" "%GAME%" -q -W %MISSION_ARGS%
popd
start "VR Spectator" "%VIEWER%" %VIEWER_ARGS%
endlocal
