# BUILD & INSTALLATION GUIDE

This document collects *all* build and installation details in one place so the main `README.md` can stay lean.

---

## 1. Supported Platforms

AVC is developed primarily on Linux, but it should compile fine on macOS and the BSD family as long as the prerequisites are met.  Windows support is on the roadmap via **MSYS2/MinGW**.

| Platform | Status                  |
|----------|-------------------------|
| Linux (x86-64) | ✅ Supported             |
| Linux (ARM64)  | ❌ Unsupported (v0.1.5+) |
| macOS (Intel & Apple-Silicon) | ⚠️ Untested             |
| Windows (MSYS2) | 🚧 Untested             |

---

## 2. Prerequisites

| Dependency | Minimum Version | Ubuntu / Debian | Fedora / RHEL | Arch / Manjaro |
|------------|-----------------|-----------------|---------------|----------------|
| **GCC / Clang** | GCC 7  or Clang 6 | `build-essential` or `clang` | `gcc` / `clang` | `base-devel` |
| **CMake** | 3.20 | `cmake` | `cmake` | `cmake` |
| **OpenSSL** | 1.1.1 | `libssl-dev` | `openssl-devel` | `openssl` |
| **libdeflate** | 1.0 | `libdeflate-dev` | `libdeflate-devel` | `libdeflate` |
| **OpenMP** | – | Comes with GCC/Clang | Comes with GCC/Clang | In toolchain |

> Tip: On very old distributions you may need to install a newer CMake from Kitware’s APT/Yum repos.

### Installing All Dependencies Quickly

Ubuntu / Debian:
```bash
sudo apt update
sudo apt install build-essential cmake libssl-dev libdeflate-dev clang
```

Fedora / RHEL:
```bash
sudo dnf groupinstall "Development Tools"
sudo dnf install cmake openssl-devel libdeflate-devel clang
```

Arch / Manjaro:
```bash
sudo pacman -S --needed base-devel cmake openssl libdeflate clang
```

macOS (Homebrew):
```bash
brew install cmake openssl libdeflate llvm
```

---

## 3. Building AVC

Always create a **separate build directory** and remember to build in **Release** mode – optimisation is required for some compiler intrinsics.

```bash
# Clone
 git clone https://github.com/assembler-0/AVC.git
 cd AVC-ArchiveVersionControl

# Configure & build (Release)
 mkdir build && cd build
 cmake -DCMAKE_BUILD_TYPE=Release ..  # -DCMAKE_INSTALL_PREFIX=<custom/path>
 make -j$(nproc)

# Optionally run tests
 make test

# Optional: install system-wide (requires root privileges)
 sudo make install
```

### Quick Sanity Check
```bash
./avc version   # Should print version and detected thread-count
```

---

## 4. Debug Builds

For development enable sanitizers and extra warnings:
```bash
mkdir build-debug && cd build-debug
cmake -DCMAKE_BUILD_TYPE=Debug -DAVC_ENABLE_SANITIZERS=ON ..
make -j$(nproc)
```

---

## 5. Uninstallation
If you installed AVC system-wide using `make install`, simply remove the installed files:
```bash
sudo xargs rm < install_manifest.txt
```

CMake generates `install_manifest.txt` in the build directory with the list of installed paths.

---

## 6. Troubleshooting

* **Missing `<omp.h>`** – your compiler was built without OpenMP support. Install `libgomp` / `clang-openmp` or rebuild the compiler with OpenMP enabled.
* **Undefined reference to `BLAKE3`** – ensure the bundled BLAKE3 sources are compiled; delete the build directory and re-run CMake.
* **Linker errors on older glibc** – try building with `-DUSE_BUNDLED_LZMA=ON` to avoid incompatible system libraries.

If you encounter problems that are not covered here, please open a GitHub issue. Pull requests with fixes are very welcome!
