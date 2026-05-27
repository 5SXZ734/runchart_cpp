@echo off
setlocal

set "TRIPLET=x64-windows"

if not defined VCPKG_ROOT (
    echo Error: VCPKG_ROOT is not set.
    echo.
    echo Set it once on this machine, for example:
    echo   setx VCPKG_ROOT D:\path\to\vcpkg
    echo.
    echo Then close and reopen this terminal.
    exit /b 1
)

if not exist "%VCPKG_ROOT%\vcpkg.exe" (
    echo Error: vcpkg.exe not found under VCPKG_ROOT:
    echo   %VCPKG_ROOT%
    exit /b 1
)

if not exist "%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" (
    echo Error: vcpkg CMake toolchain not found under VCPKG_ROOT:
    echo   %VCPKG_ROOT%
    exit /b 1
)

where cmake >nul 2>nul
if errorlevel 1 (
    echo Error: cmake.exe not found in PATH.
    echo Install CMake or run from a Visual Studio Developer Command Prompt.
    exit /b 1
)

if not exist "third_party" mkdir "third_party"
if not exist "third_party\httplib.h" (
    echo Downloading cpp-httplib...
    powershell -NoProfile -ExecutionPolicy Bypass -Command ^
        "Invoke-WebRequest -Uri 'https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h' -OutFile 'third_party\httplib.h'"
    if errorlevel 1 exit /b %errorlevel%
)

echo Using VCPKG_ROOT=%VCPKG_ROOT%
echo Installing vcpkg dependencies...
"%VCPKG_ROOT%\vcpkg.exe" install grpc:%TRIPLET% protobuf:%TRIPLET% nlohmann-json:%TRIPLET% sqlite3:%TRIPLET% taglib:%TRIPLET%
if errorlevel 1 exit /b %errorlevel%

echo.
echo Configuring CMake...
cmake --fresh --preset windows-vcpkg
if errorlevel 1 exit /b %errorlevel%

echo.
echo Building Release configuration...
cmake --build --preset windows-vcpkg-release
if errorlevel 1 exit /b %errorlevel%

echo.
echo Build completed successfully.
echo Server: build\windows-vcpkg\Release\runchart_server.exe
echo Client: build\windows-vcpkg\Release\runchart_client.exe
