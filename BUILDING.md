# Building VM Manager

VM Manager builds from one CMake project on all three platforms. `libvirt` and
`qemu-img` are **optional** at build time — without them the app runs against
the built-in mock backend, which is perfect for UI work.

- **Qt** 6.5 or newer (Quick, QuickControls2)
- **CMake** 3.21+
- A **C++20** compiler (Clang, GCC, or MSVC)

---

## macOS (Homebrew) — recommended path

```bash
# 1. Toolchain + optional runtime deps
brew install qt cmake ninja
brew install libvirt qemu          # optional: enables the real backend + import

# 2. Configure & build
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build --parallel

# 3. Run
./build/vm-manager.app/Contents/MacOS/vm-manager
# ...or just: open ./build/vm-manager.app
```

To run against the mock backend explicitly (no libvirt needed):

```bash
VMM_BACKEND=mock ./build/vm-manager.app/Contents/MacOS/vm-manager
```

Package a distributable `.app` + `.dmg`:

```bash
cmake --build build --target package    # uses macdeployqt under the hood
```

> **Note on local VMs on macOS:** libvirt on macOS drives QEMU (with the HVF
> accelerator). `brew services start libvirt` starts the daemon; VM Manager then
> connects to `qemu:///session` by default. Remote Linux/KVM hosts work over
> `qemu+ssh://` with no local hypervisor at all.

---

## Linux (Debian/Ubuntu)

```bash
sudo apt install qt6-base-dev qt6-declarative-dev \
  qml6-module-qtquick-controls qml6-module-qtquick-layouts \
  cmake ninja-build g++ \
  libvirt-dev qemu-utils               # last line optional (real backend + import)

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/vm-manager
```

Fedora:

```bash
sudo dnf install qt6-qtbase-devel qt6-qtdeclarative-devel \
  cmake ninja-build gcc-c++ libvirt-devel qemu-img
```

Package an AppImage:

```bash
cmake --build build --target package
```

---

## Windows (MSVC + Qt online installer)

```powershell
# Install Qt 6 via the official online installer (msvc2022_64 component),
# plus "Desktop development with C++" from Visual Studio Build Tools.

cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.7.0/msvc2022_64"
cmake --build build --config Release --parallel
.\build\Release\vm-manager.exe
```

libvirt on Windows connects to remote hosts over `qemu+ssh://` /
`qemu+tls://`; a local hypervisor path (Hyper-V/WHPX) is a later phase. Package
an installer:

```powershell
cmake --build build --config Release --target package   # windeployqt + NSIS
```

---

## Build options

| CMake option | Default | Effect |
|---|---|---|
| `VMM_WITH_LIBVIRT` | `AUTO` | `ON`/`OFF`/`AUTO` — force or auto-detect the real libvirt backend. |
| `VMM_BUILD_TESTS` | `ON` | Build the unit/UI smoke tests. |
| `CMAKE_BUILD_TYPE` | `Release` | Standard CMake build type. |

Runtime:

| Env var | Effect |
|---|---|
| `VMM_BACKEND=mock` | Force the mock backend even if libvirt is available. |
| `VMM_CONNECT=qemu+ssh://user@host/system` | Auto-connect to a URI on launch. |

---

## Running the tests

```bash
cmake -S . -B build -DVMM_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```
