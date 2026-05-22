@echo off
setlocal enabledelayedexpansion

set VCPKG_ROOT=C:\WORK\external\vcpkg
set TRIPLET=x64-windows

if not exist "%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" (
    echo Error: Could not find vcpkg toolchain at "%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
    echo.
    echo Please update the VCPKG_ROOT variable in this script to match your vcpkg installation.
    exit /b 1
)

echo Installing vcpkg dependencies...
"%VCPKG_ROOT%\vcpkg.exe" install grpc:%TRIPLET% protobuf:%TRIPLET% nlohmann-json:%TRIPLET%
if errorlevel 1 (
    echo Error: vcpkg install failed
    exit /b %errorlevel%
)

echo.
echo Configuring CMake...
cmake --preset windows-vcpkg
if errorlevel 1 (
    echo Error: CMake configure failed
    exit /b %errorlevel%
)

echo.
echo Building Release configuration...
cmake --build --preset windows-vcpkg-release
if errorlevel 1 (
    echo Error: CMake build failed
    exit /b %errorlevel%
)

echo.
echo ============================================================
echo Build completed successfully!
echo ============================================================
echo.
echo Server executable:
echo   build\windows-vcpkg\Release\runchart_server.exe
echo.
echo Client executable:
echo   build\windows-vcpkg\Release\runchart_client.exe
echo.
echo Example usage:
echo   1. Start server in one terminal:
echo      build\windows-vcpkg\Release\runchart_server.exe
echo.
echo   2. Run client in another terminal:
echo      build\windows-vcpkg\Release\runchart_client.exe localhost:3030 data.json
echo.
echo   3. To connect to remote server (e.g., NAS):
echo      build\windows-vcpkg\Release\runchart_client.exe 192.168.1.4:3030 data.json
echo.
