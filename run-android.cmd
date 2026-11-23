@echo off
setlocal
cd /d "%~dp0"
if not defined ANDROID_HOME if defined ANDROID_SDK_ROOT set "ANDROID_HOME=%ANDROID_SDK_ROOT%"
if not defined ANDROID_HOME set "ANDROID_HOME=%LOCALAPPDATA%\Android\Sdk"
set "ADB=%ANDROID_HOME%\platform-tools\adb.exe"
if not exist "%ADB%" (echo Run setup-android.cmd first. & exit /b 1)
"%ADB%" get-state >nul || goto device_error
set "DEVICE_ABIS="
for /f "delims=" %%A in ('""%ADB%" shell getprop ro.product.cpu.abilist"') do set "DEVICE_ABIS=%%A"
set "GAME_ABI="
for %%A in (%DEVICE_ABIS:,= %) do if not defined GAME_ABI (
    if "%%A"=="arm64-v8a" set "GAME_ABI=arm64-v8a"
    if "%%A"=="armeabi-v7a" set "GAME_ABI=armeabi-v7a"
    if "%%A"=="x86" set "GAME_ABI=x86"
)
if not defined GAME_ABI (echo Unsupported device CPU list: %DEVICE_ABIS%. Use an ARM Android phone or an emulator with 32-bit x86 support. & exit /b 1)
echo Building for %GAME_ABI%...
call gradlew.bat :app:assembleDebug "-PgameAbi=%GAME_ABI%" -PresourcePreview
if errorlevel 1 exit /b 1
"%ADB%" install -r "proj.android\app\build\outputs\apk\debug\app-debug.apk"
if errorlevel 1 exit /b 1
"%ADB%" shell am start -W -n com.iceadventurequest.frosttrail.resources/com.iceadventurequest.game.GameActivity
exit /b %errorlevel%
:device_error
"%ADB%" devices -l
echo Connect one unlocked Android phone, enable USB debugging and allow this computer on the phone.
echo For multiple devices, set ANDROID_SERIAL to the device serial in this terminal.
exit /b 1