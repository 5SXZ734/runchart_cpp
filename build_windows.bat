@echo off
setlocal

set VCPKG_ROOT=C:\WORK\external\vcpkg
set TRIPLET=x64-windows

if not exist "%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" (
    echo Could not find vcpkg toolchain at "%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
    exit /b 1
)

"%VCPKG_ROOT%\vcpkg.exe" install grpc:%TRIPLET% protobuf:%TRIPLET%
if errorlevel 1 exit /b %errorlevel%

cmake --preset windows-vcpkg
if errorlevel 1 exit /b %errorlevel%

cmake --build --preset windows-vcpkg-release
if errorlevel 1 exit /b %errorlevel%

echo.
echo Built: build\windows-vcpkg\Release\runchart_demo.exe
echo Run:   build\windows-vcpkg\Release\runchart_demo.exe data.json
