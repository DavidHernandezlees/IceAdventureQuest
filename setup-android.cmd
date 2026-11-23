@echo off
setlocal
if not defined ANDROID_HOME if defined ANDROID_SDK_ROOT set "ANDROID_HOME=%ANDROID_SDK_ROOT%"
if not defined ANDROID_HOME set "ANDROID_HOME=%LOCALAPPDATA%\Android\Sdk"
if defined JAVA_HOME set "PATH=%JAVA_HOME%\bin;%PATH%"
where java.exe >nul 2>nul || (echo Install JDK 21, then reopen this terminal. & exit /b 1)
set "SDKMANAGER=%ANDROID_HOME%\cmdline-tools\latest\bin\sdkmanager.bat"
if exist "%SDKMANAGER%" goto install
for /d %%D in ("%ANDROID_HOME%\cmdline-tools\*") do if exist "%%~fD\bin\sdkmanager.bat" set "SDKMANAGER=%%~fD\bin\sdkmanager.bat"
if exist "%SDKMANAGER%" goto install
echo Installing Google's Android command-line tools in "%ANDROID_HOME%\cmdline-tools\12.0"...
powershell.exe -NoProfile -Command "$ErrorActionPreference = 'Stop'; [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; $tmp = Join-Path ([IO.Path]::GetTempPath()) ([guid]::NewGuid()); $null = New-Item -Path $tmp -ItemType Directory; try { $archive = Join-Path $tmp 'tools.zip'; (New-Object Net.WebClient).DownloadFile('https://dl.google.com/android/repository/commandlinetools-win-11076708_latest.zip', $archive); if ((Get-FileHash -Algorithm SHA1 -Path $archive).Hash -ne '3d2917302740f476999a091bc5558837c7a863c5') { throw 'Android command-line tools checksum mismatch.' }; Expand-Archive -LiteralPath $archive -DestinationPath $tmp; $parent = Join-Path $env:ANDROID_HOME 'cmdline-tools'; $null = New-Item -Path $parent -ItemType Directory -Force; $dest = Join-Path $parent '12.0'; if (Test-Path $dest) { throw ('An incomplete install already exists at ' + $dest + '. Repair it in SDK Manager or choose a different ANDROID_HOME.') }; Move-Item -LiteralPath (Join-Path $tmp 'cmdline-tools') -Destination $dest } finally { Remove-Item -LiteralPath $tmp -Recurse -Force }"
if errorlevel 1 exit /b 1
set "SDKMANAGER=%ANDROID_HOME%\cmdline-tools\12.0\bin\sdkmanager.bat"
:install
echo Android SDK location: "%ANDROID_HOME%"
call "%SDKMANAGER%" "--sdk_root=%ANDROID_HOME%" --include_obsolete "platform-tools" "platforms;android-35" "build-tools;34.0.0" "cmake;3.22.1" "ndk;19.2.5345600"
if errorlevel 1 exit /b 1
for %%F in ("platform-tools\adb.exe" "platforms\android-35\android.jar" "build-tools\34.0.0\aapt2.exe" "cmake\3.22.1\bin\cmake.exe" "ndk\19.2.5345600\source.properties") do (
    if not exist "%ANDROID_HOME%\%%~F" (
        echo Missing SDK component: %%~F. Run setup again and accept the requested SDK licenses.
        exit /b 1
    )
)
echo Android SDK setup complete. Enable USB debugging on your phone and run run-android.cmd.