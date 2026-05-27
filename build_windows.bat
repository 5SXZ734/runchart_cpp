@echo off
setlocal

set "TRIPLET=x64-windows"

if not defined VCPKG_ROOT (
    echo Error: VCPKG_ROOT is not set.
    echo.
    echo Set it once, for example:
    echo   setx VCPKG_ROOT D:\WORK\external\vcpkg
    exit /b 1
)

if not exist "%VCPKG_ROOT%\vcpkg.exe" (
    echo Error: Could not find vcpkg.exe at:
    echo   "%VCPKG_ROOT%\vcpkg.exe"
    exit /b 1
)

if not exist "%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" (
    echo Error: Could not find vcpkg toolchain at:
    echo   "%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
    exit /b 1
)

where cmake >nul 2>nul
if errorlevel 1 (
    echo Error: cmake.exe not found in PATH.
    echo Install CMake or add it to PATH.
    exit /b 1
)

echo Using VCPKG_ROOT=%VCPKG_ROOT%
echo Using TRIPLET=%TRIPLET%
echo.

echo Installing vcpkg dependencies...
"%VCPKG_ROOT%\vcpkg.exe" install ^
    grpc:%TRIPLET% ^
    protobuf:%TRIPLET% ^
    nlohmann-json:%TRIPLET% ^
    sqlite3:%TRIPLET% ^
    taglib:%TRIPLET%

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

