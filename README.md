# RunChart C++ / CMake port

This is a C++17 port of the uploaded Java Maven gRPC demo.

## What it preserves

- `SnapShot`: unary request returning the latest measurement, or a default value.
- `SendMeasurements`: client-streaming upload of `DataPoint` messages.
- `Monitor`: server-streaming subscription to received measurements.
- `SendAndCheck`: bidirectional stream; each out-of-spec measurement returns a `Warning`.
- Same port: `3030`.
- Same sample input: `data.json`.

## Dependencies

Install CMake, a C++17 compiler, protobuf, and gRPC C++.

### vcpkg example

```bash
vcpkg install grpc protobuf
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

### Linux package-manager example

Package names vary by distro, but usually you need development packages for `grpc++`, `protobuf`, and `protoc`.

```bash
cmake -S . -B build
cmake --build build -j
```

## Run

From the project root:

```bash
./build/runchart_demo data.json
```

On Windows with a multi-config generator:

```powershell
.\build\Release\runchart_demo.exe data.json
```

## Notes

The Java `Main` starts `monitor()` and `sendAndCheck()` concurrently. The C++ port does the same, but shuts the server down after `sendAndCheck()` completes so the demo exits cleanly.
