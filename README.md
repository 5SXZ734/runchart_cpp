# RunChart C++ / CMake Demo

This is a C++17 port of the Java Maven gRPC demo, converted to run on Windows with Visual Studio, and ready for Linux Docker deployment.

## Architecture

- **Server** (`runchart_server`): Listens on `0.0.0.0:3030`
  - Receives measurements via gRPC streaming
  - Broadcasts measurements to subscribers
  - Returns warnings for out-of-spec measurements
  
- **Client** (`runchart_client`): Connects to remote server
  - Loads measurements from JSON file
  - Sends measurements to server
  - Receives and prints warnings
  - Accepts server address and data file from command line

## Features

- **SnapShot**: Unary RPC returning the latest measurement
- **SendMeasurements**: Client-streaming upload of measurements
- **Monitor**: Server-streaming broadcast of all measurements
- **SendAndCheck**: Bidirectional streaming with warning responses
- Service port: `3030` (same as original Java port)


## New Windows 11 Setup

For a complete step-by-step setup (WSL2, Docker Desktop, native Visual Studio/vcpkg, and verification), see **`WINDOWS_SETUP_GUIDE.md`**.

## Prerequisites

### Windows Development

- Visual Studio 2022 or later (or any CMake-compatible generator)
- CMake 3.20+
- vcpkg with gRPC, protobuf, and nlohmann-json packages
- vcpkg root: `C:\WORK\external\vcpkg` (or update CMakePresets.json)

### Linux / Docker

- CMake 3.20+
- C++17 compiler (GCC 7+ or Clang 5+)
- System packages: `libgrpc-dev`, `libprotobuf-dev`, `protobuf-compiler`, `nlohmann-json3-dev`

## Building on Windows

### Quick build with batch script:

```batch
build_windows.bat
```

This script will:
1. Install dependencies using vcpkg
2. Configure CMake with Visual Studio generator
3. Build Release configuration
4. Output executables to `build\windows-vcpkg\Release\`

### Manual build:

```batch
cd C:\path\to\runchart_cpp

REM Install dependencies
C:\WORK\external\vcpkg\vcpkg install grpc:x64-windows protobuf:x64-windows nlohmann-json:x64-windows

REM Configure and build
cmake --preset windows-vcpkg
cmake --build --preset windows-vcpkg-release

REM Executables are in: build\windows-vcpkg\Release\
```

## Building on Linux / Docker

```bash
sudo apt-get update
sudo apt-get install -y cmake protobuf-compiler libgrpc-dev libgrpc++-dev nlohmann-json3-dev

cd /path/to/runchart_cpp
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Executables are in: build/
```


### Build Docker image manually

```bash
docker build -t runchart-server:latest .
docker save -o runchart-server-latest.tar runchart-server:latest
```

Load/run on target host (example):

```bash
docker load -i runchart-server-latest.tar
docker run --rm -p 3030:3030 -p 8080:8080 --env-file .env runchart-server:latest
```

## Running

### Start the Server (Windows)

```batch
build\windows-vcpkg\Release\runchart_server.exe
```

Output:
```
Server listening on 0.0.0.0:3030
Press Ctrl+C to stop.
```

### Start the Server (Linux)

```bash
./build/runchart_server
```

### Run the Client (Windows)

**Local server:**
```batch
build\windows-vcpkg\Release\runchart_client.exe localhost:3030 data.json
```

**Remote server (e.g., Synology NAS at 192.168.1.4):**
```batch
build\windows-vcpkg\Release\runchart_client.exe 192.168.1.4:3030 data.json
```

### Run the Client (Linux)

```bash
./build/runchart_client localhost:3030 data.json
```

## File Structure

```
.
├── CMakeLists.txt                   # Build configuration
├── CMakePresets.json                # Visual Studio / Ninja presets
├── build_windows.bat                # Quick Windows build script
├── README.md                         # This file
├── data.json                         # Sample measurements
├── proto/
│   └── runchart.proto              # gRPC service definition
└── src/
    ├── measurement.h               # Measurement data structure
    ├── measurement.cpp
    ├── run_chart_service.h         # gRPC service implementation
    ├── run_chart_service.cpp
    ├── run_chart_client.h          # gRPC client implementation
    ├── run_chart_client.cpp
    ├── server_main.cpp             # Server executable entry point
    └── client_main.cpp             # Client executable entry point
```

## Generated Files (Build Directory)

During build, CMake generates:
- `build/generated/runchart.pb.cc/.h` — Protobuf message serialization
- `build/generated/runchart.grpc.pb.cc/.h` — gRPC service stubs

These are **never** committed to the repository and are generated fresh on each build if `proto/runchart.proto` changes.

## Customization

### Change Server Port

Edit `src/server_main.cpp` and change:
```cpp
const std::string address = "0.0.0.0:3030";  // Change 3030 to your port
```

Then rebuild.

### Use vcpkg with Different Triplet

Edit `CMakePresets.json` and change:
```json
"VCPKG_TARGET_TRIPLET": "x64-windows-static"  // or any other triplet
```

### Change gRPC Credentials (for production)

Edit `src/server_main.cpp` and `src/client_main.cpp`:
```cpp
// Current (insecure, LAN demo only):
grpc::InsecureServerCredentials()
grpc::InsecureChannelCredentials()

// For production, use:
// grpc::SslServerCredentials(...)
// grpc::SslChannelCredentials(...)
```

## Troubleshooting

### "Could not find gRPC plugin executable"
- Ensure vcpkg packages are installed correctly
- Verify CMakePresets.json points to the correct vcpkg root

### "Could not open data.json"
- Run client from the directory containing `data.json`, or provide the full path:
  ```batch
  build\windows-vcpkg\Release\runchart_client.exe localhost:3030 C:\full\path\to\data.json
  ```

### Connection refused on remote server
- Verify the server is running and listening on the specified port
- Check firewall rules
- Ensure the IP address and port are correct

### "Invalid auth token" from client
- Set `RUNCHART_AUTH_TOKEN` (or `RUNCHART_AUTH_SECRET`) in the client environment to the same value used by the server `RUNCHART_AUTH_SECRET`.
- Example (Windows cmd): `set RUNCHART_AUTH_TOKEN=your-secret`
- Example (Linux): `export RUNCHART_AUTH_TOKEN=your-secret`

### CMake configuration fails on Windows
- Install Visual Studio build tools
- Ensure CMake 3.20+ is in PATH
- Verify vcpkg installation

## Notes

- All protobuf/gRPC files are generated into the build directory, not the source tree
- Timestamps are parsed as ISO 8601 UTC (e.g., `2024-04-08T12:00:00.000Z`)
- JSON loading uses `nlohmann::json` library
- Project uses C++17 with no external dependencies except gRPC, protobuf, and nlohmann-json
- Insecure gRPC credentials are suitable only for LAN demos; use proper TLS/SSL for production

## Configuration

Copy `.env.example` to `.env` (or set `RUNCHART_CONFIG_FILE`) and provide gRPC address, HTTP bind/port, NAS path, and auth secret.

The service exposes:
- `GET /health` -> `ok`
- `GET /metrics` -> basic counters in Prometheus text format
