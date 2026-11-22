@echo off
setlocal

rem | Builds raylib from its source for every platform, with "Build.bat raylib" in each platform's folder (Framework\
rem | Automations\Platforms), then replaces include and LICENSE.txt. Platforms whose tools are missing are skipped.
rem |   UpdateRaylib.bat [source folder or .zip, or --download] [platform names] [--no-pause]
rem | --download takes the raylib release that matches include\raylib.h.

set "RAYLIB_DIR=%~dp0"
set "RAYLIB_DIR=%RAYLIB_DIR:~0,-1%"
for %%R in ("%RAYLIB_DIR%\..\..") do set "ROOT=%%~fR"
set "WORK=%TEMP%\RaylibUpdate_%RANDOM%%RANDOM%"
set "PAUSE=pause"
set "INPUT="
set "PLATFORMS="
:Arguments
if "%~1"=="" goto :ArgumentsDone
if /i "%~1"=="--no-pause" (set "PAUSE=") else if not defined INPUT (set "INPUT=%~1") else (set "PLATFORMS=%PLATFORMS% %~1")
shift
goto :Arguments
:ArgumentsDone
if not defined PLATFORMS (
    pushd "%ROOT%\Framework\Automations\Platforms"
    for /d %%P in (*) do findstr /b /i /c:":raylib" "%%P\Build.bat" >nul 2>&1 && call set "PLATFORMS=%%PLATFORMS%% %%P"
    popd
)

if defined INPUT goto :HaveInput
echo Drag raylib's source folder or .zip onto this window, then press Enter.
echo (A release's "Source code (zip)" from github.com/raysan5/raylib/releases, or leave it empty to download the one
echo that matches Libraries\Raylib\include.)
set /p "INPUT=Source: "
if not defined INPUT set "INPUT=--download"
:HaveInput
set "INPUT=%INPUT:"=%"
mkdir "%WORK%\Unpacked"

if /i not "%INPUT%"=="--download" goto :CheckInput
for /f "tokens=3" %%V in ('findstr /c:"#define RAYLIB_VERSION " "%RAYLIB_DIR%\include\raylib.h"') do set "VERSION=%%~V"
set "INPUT=%WORK%\raylib-%VERSION%.zip"
echo [Update raylib] Downloading raylib %VERSION%...
powershell -NoProfile -Command "$ProgressPreference = 'SilentlyContinue'; Invoke-WebRequest -UseBasicParsing -Uri ('https://github.com/raysan5/raylib/archive/refs/tags/' + $env:VERSION + '.zip') -OutFile $env:INPUT"
if errorlevel 1 goto :Failed

:CheckInput
if not exist "%INPUT%" goto :NoInput
echo [Update raylib] Copying the source...
if /i "%INPUT:~-4%"==".zip" goto :Unzip
robocopy "%INPUT%" "%WORK%\Unpacked\Source" /E /NFL /NDL /NJH /NJS /NP /XD .git examples >nul
if errorlevel 8 goto :Failed
goto :FindSource

:Unzip
tar -xf "%INPUT%" -C "%WORK%\Unpacked" || goto :Failed

:FindSource
set "SOURCE="
if exist "%WORK%\Unpacked\CMakeLists.txt" set "SOURCE=%WORK%\Unpacked"
pushd "%WORK%\Unpacked"
for /d %%D in (*) do if exist "%%D\CMakeLists.txt" if exist "%%D\src\raylib.h" set "SOURCE=%WORK%\Unpacked\%%D"
popd
if not defined SOURCE goto :NotSource
if not exist "%SOURCE%\src\raylib.h" goto :NotSource

set "VERSION="
for /f "tokens=3" %%V in ('findstr /c:"#define RAYLIB_VERSION " "%SOURCE%\src\raylib.h"') do set "VERSION=%%~V"
echo [Update raylib] Found raylib %VERSION%.

call :FindTools
rem The file formats raylib leaves off by default, turned on for every platform
set "RAYLIB_FORMATS=SUPPORT_FILEFORMAT_JPG SUPPORT_FILEFORMAT_TGA SUPPORT_FILEFORMAT_PSD SUPPORT_FILEFORMAT_HDR SUPPORT_FILEFORMAT_PIC SUPPORT_FILEFORMAT_PNM SUPPORT_FILEFORMAT_KTX SUPPORT_FILEFORMAT_ASTC SUPPORT_FILEFORMAT_PKM SUPPORT_FILEFORMAT_PVR SUPPORT_FILEFORMAT_BDF SUPPORT_FILEFORMAT_FLAC"
set "UPDATED="
set "SKIPPED="
for %%P in (%PLATFORMS%) do (
    findstr /b /i /c:":raylib" "%ROOT%\Framework\Automations\Platforms\%%P\Build.bat" >nul 2>&1
    if errorlevel 1 (
        echo [Update raylib][Warning] %%P's Build.bat has no raylib part, in Framework\Automations\Platforms\%%P.
    ) else (
        call "%ROOT%\Framework\Automations\Platforms\%%P\Build.bat" raylib
        if errorlevel 2 goto :Failed
        if errorlevel 1 (call set "SKIPPED=%%SKIPPED%% %%P") else (call set "UPDATED=%%UPDATED%% %%P")
    )
)
if not defined UPDATED goto :Failed

echo [Update raylib] Replacing include and LICENSE.txt...
copy /Y "%SOURCE%\src\raylib.h" "%RAYLIB_DIR%\include\" >nul || goto :Failed
if exist "%SOURCE%\LICENSE" copy /Y "%SOURCE%\LICENSE" "%RAYLIB_DIR%\LICENSE.txt" >nul
rem New headers keep their old dates: clear every compiled cache, so nothing stale is reused
if exist "%ROOT%\Builds\Cache" for /d /r "%ROOT%\Builds\Cache" %%O in (Objects) do if exist "%%O" rmdir /s /q "%%O"

rmdir /s /q "%WORK%"
echo.
echo [Update raylib] Done: raylib %VERSION% for%UPDATED%.
if defined SKIPPED echo [Update raylib] Skipped:%SKIPPED%. Their lib folders still hold the raylib they had.
%PAUSE%
exit /b 0

:FindTools
rem Visual Studio, and its cmake and ninja, for Windows; the other platforms compile raylib's files directly
set "VS_PATH="
set "VS_INSTALLER=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
if exist "%VS_INSTALLER%\vswhere.exe" for /f "delims=" %%P in ('call "%VS_INSTALLER%\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul') do set "VS_PATH=%%P"
set "VCVARSALL=%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat"
set "CMAKE_TOOL="
set "NINJA_TOOL="
for %%X in (cmake.exe) do if not "%%~$PATH:X"=="" set "CMAKE_TOOL=%%~$PATH:X"
for %%X in (ninja.exe) do if not "%%~$PATH:X"=="" set "NINJA_TOOL=%%~$PATH:X"
set "VS_CMAKE=%VS_PATH%\Common7\IDE\CommonExtensions\Microsoft\CMake"
if not defined CMAKE_TOOL if exist "%VS_CMAKE%\CMake\bin\cmake.exe" set "CMAKE_TOOL=%VS_CMAKE%\CMake\bin\cmake.exe"
if not defined NINJA_TOOL if exist "%VS_CMAKE%\Ninja\ninja.exe" set "NINJA_TOOL=%VS_CMAKE%\Ninja\ninja.exe"
if not defined CMAKE_TOOL set "NINJA_TOOL="
exit /b 0

:NoInput
echo [Update raylib][Error] "%INPUT%" was not found.
goto :Cleanup

:NotSource
echo [Update raylib][Error] That is not raylib's source: no CMakeLists.txt next to src\raylib.h. The raylib-*_win64_msvc*.zip release is prebuilt, not source; use the release's "Source code (zip)" or GitHub's Download ZIP.
goto :Cleanup

:Failed
echo [Update raylib][Error] Stopped, see the errors above.

:Cleanup
if exist "%WORK%" rmdir /s /q "%WORK%"
%PAUSE%
exit /b 1
