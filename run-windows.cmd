@echo off
setlocal
cd /d "%~dp0"
set "CMAKE=cmake.exe"
where cmake.exe >nul 2>nul
if not errorlevel 1 goto configure
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto help
for /f "usebackq delims=" %%D in (`"%VSWHERE%" -products * -version "[17.0,18.0)" -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "CMAKE=%%D\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not exist "%CMAKE%" goto help
:configure
"%CMAKE%" -S . -B build-win32 -G "Visual Studio 17 2022" -A Win32
if errorlevel 1 exit /b 1
"%CMAKE%" --build build-win32 --config Release --parallel
if errorlevel 1 exit /b 1
start "" "%CD%\build-win32\bin\IceAdventureQuest\Release\IceAdventureQuest.exe"
exit /b 0
:help
echo Install Visual Studio 2022 with Desktop development with C++, MSVC v143 x86/x64, a Windows SDK and C++ CMake tools for Windows.
exit /b 1
