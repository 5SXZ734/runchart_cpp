# Windows 11 Full Setup Guide (WSL + Docker + Native Build)

This guide sets up a **new Windows 11 machine** to build and run this project in three ways:

1. **WSL (recommended for daily development)**
2. **Docker (recommended for consistent server runtime)**
3. **Native Windows + Visual Studio + vcpkg (recommended when you need `.exe` artifacts)**

---

## 0) Pre-flight checklist

- Windows 11 fully updated
- Local admin access on the machine
- 20+ GB free disk (WSL image + Docker cache + build files)
- GitHub access to clone this repo

Open **PowerShell as Administrator** for steps that explicitly say Admin.

---

## 1) Install core tooling on Windows

Install the following first:

- **Git for Windows**
- **Visual Studio 2022 Community** (Desktop development with C++)
- **CMake** (optional if using VS bundled CMake, but recommended)
- **Docker Desktop for Windows**
- **Windows Terminal** (if not already installed)

### 1.1 Visual Studio workload/components

In Visual Studio Installer, select:

- Workload: **Desktop development with C++**
- Individual components:
  - MSVC v143 toolset
  - Windows 11 SDK
  - C++ CMake tools for Windows

---

## 2) Enable and install WSL2

Run in **PowerShell (Admin)**:

```powershell
wsl --install -d Ubuntu
```

Then reboot if prompted.

After reboot, launch Ubuntu once and create your Linux username/password.

Verify:

```powershell
wsl -l -v
```

You should see Ubuntu with **VERSION 2**.

If needed:

```powershell
wsl --set-default-version 2
```

---

## 3) Configure Docker Desktop to use WSL2

1. Open Docker Desktop
2. Settings → General: enable **Use the WSL 2 based engine**
3. Settings → Resources → WSL Integration: enable your Ubuntu distro
4. Apply & Restart

Verify from Ubuntu shell:

```bash
docker version
docker info
```

---

## 4) Clone the repository

Recommended: clone inside Linux filesystem for best performance.

In Ubuntu (WSL):

```bash
mkdir -p ~/dev
cd ~/dev
git clone <YOUR_REPO_URL> runchart_cpp
cd runchart_cpp
```

---

## 5) Install Linux build dependencies inside WSL

In Ubuntu:

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  ninja-build \
  pkg-config \
  protobuf-compiler \
  libprotobuf-dev \
  libgrpc-dev \
  libgrpc++-dev \
  nlohmann-json3-dev \
  libsqlite3-dev \
  libtag1-dev \
  libvlc-dev
```

Optional helpful tools:

```bash
sudo apt-get install -y gdb valgrind curl jq
```

---

## 6) Build and run in WSL (recommended dev workflow)

From repo root in WSL:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

Run server:

```bash
./build/runchart_server
```

In a second terminal run client:

```bash
cd ~/dev/runchart_cpp
./build/runchart_client localhost:3030 send_and_check data.json
```

Expected behavior:

- server prints that it is listening on `0.0.0.0:3030`
- client sends measurements and receives warnings when out-of-spec values appear

---

## 7) Build and run with Docker

From repo root (WSL):

```bash
docker build -t runchart-server:latest .
```

Run container:

```bash
docker run --rm -p 3030:3030 -p 8080:8080 --env-file .env runchart-server:latest
```

Health check from another terminal:

```bash
curl -s http://localhost:8080/health
```

Metrics endpoint:

```bash
curl -s http://localhost:8080/metrics
```

### 7.1 Optional: export image for another machine

```bash
docker save -o runchart-server-latest.tar runchart-server:latest
```

Load elsewhere:

```bash
docker load -i runchart-server-latest.tar
```

---

## 8) Native Windows build (Visual Studio + vcpkg)

Use this when you need native Windows binaries.

### 8.1 Install vcpkg

In **Developer PowerShell for VS 2022**:

```powershell
cd C:\WORK\external
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
```

### 8.2 Install dependencies via vcpkg

```powershell
.\vcpkg install grpc:x64-windows protobuf:x64-windows nlohmann-json:x64-windows sqlite3:x64-windows taglib:x64-windows
```

### 8.3 Install VLC/libVLC for client playback

Install or extract VLC 3.x so the libVLC SDK is available at:

```powershell
D:\WORK\external\vlc-3.0.23
```

The CMake build uses this as the default `RUNCHART_VLC_ROOT`. If your VLC install is elsewhere, pass `-DRUNCHART_VLC_ROOT=<path>` during CMake configure. At runtime, ensure Windows can find `libvlc.dll`:

```powershell
$env:PATH = "D:\WORK\external\vlc-3.0.23;$env:PATH"
```

### 8.4 Build this project

From repo root in Windows shell:

```powershell
cmake --preset windows-vcpkg
cmake --build --preset windows-vcpkg-release
```

Artifacts:

- `build\windows-vcpkg\Release\runchart_server.exe`
- `build\windows-vcpkg\Release\runchart_client.exe`

### 8.5 Run on Windows

Server:

```powershell
build\windows-vcpkg\Release\runchart_server.exe
```

Client:

```powershell
build\windows-vcpkg\Release\runchart_client.exe localhost:3030 send_and_check data.json
build\windows-vcpkg\Release\runchart_client.exe localhost:3030 list_tracks
build\windows-vcpkg\Release\runchart_client.exe localhost:3030 play 42
```

---

## 9) Environment configuration (`.env`)

Create from template:

```bash
cp .env.example .env
```

Set values for your environment, especially:

- gRPC bind/address
- HTTP bind/port
- NAS path (if used)
- `RUNCHART_AUTH_SECRET`

For client auth from shell:

- Windows cmd: `set RUNCHART_AUTH_TOKEN=your-secret`
- PowerShell: `$env:RUNCHART_AUTH_TOKEN = "your-secret"`
- Linux/WSL: `export RUNCHART_AUTH_TOKEN=your-secret`

---

## 10) Recommended day-to-day workflow

- Develop and test in **WSL** for speed and Linux parity
- Package/runtime test in **Docker**
- Produce native Windows `.exe` only when needed via VS/vcpkg

Typical loop:

1. Pull latest code
2. Build in WSL (`cmake -S . -B build ...`)
3. Run server/client locally
4. Build Docker image for deployment test
5. If required, run Windows preset build for `.exe`

---

## 11) Troubleshooting

### WSL command not found / virtualization issues

- Ensure BIOS virtualization is enabled
- Ensure Windows features are enabled:
  - Virtual Machine Platform
  - Windows Subsystem for Linux

### Docker can’t connect from WSL

- Docker Desktop must be running
- WSL integration must be enabled for your distro
- Try Docker Desktop restart

### CMake cannot find gRPC/protobuf on Linux

Reinstall packages:

```bash
sudo apt-get install --reinstall -y libgrpc-dev libgrpc++-dev libprotobuf-dev protobuf-compiler
```

### Windows preset fails due to vcpkg path mismatch

- Confirm `CMakePresets.json` vcpkg toolchain path matches your local install
- Default in this repo assumes `C:\WORK\external\vcpkg`

### Client auth token errors

- Ensure server `RUNCHART_AUTH_SECRET` and client `RUNCHART_AUTH_TOKEN` match exactly

---

## 12) Quick verification checklist

Run all from repo root (WSL) unless noted.

1. Build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

2. Server startup:

```bash
./build/runchart_server
```

3. Client call:

```bash
./build/runchart_client localhost:3030 send_and_check data.json
```

4. Docker runtime:

```bash
docker build -t runchart-server:latest .
docker run --rm -p 3030:3030 -p 8080:8080 --env-file .env runchart-server:latest
curl -s http://localhost:8080/health
```

If all pass, your Windows 11 environment is fully ready.
