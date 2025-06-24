# BUILD & INSTALLATION GUIDE

This document collects *all* build and installation details in one place so the main `README.md` can stay lean.

---

## 1. Supported Platforms

AVC is developed primarily on Linux, but it should compile fine on macOS and the BSD family as long as the prerequisites are met.  Windows support is on the roadmap via **MSYS2/MinGW**.

| Platform | Status                  |
|----------|-------------------------|
| Linux (x86-64) | ✅ Fully Supported       |
| Linux (ARM64)  | ✅ Supported (portable build) |
| macOS (Intel & Apple-Silicon) | ✅ Supported (portable build) |
| Windows (MSYS2) | ⚠️ Untested             |

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

### Standard Build (Native Optimizations)
```bash
# Clone
git clone https://github.com/assembler-0/AVC.git
cd AVC-ArchiveVersionControl

# Configure & build (Release with native optimizations)
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# Optional: install system-wide
sudo make install
```

### Portable Build (ARM/Cross-Platform)
```bash
# For ARM, older CPUs, or cross-compilation
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DAVC_PORTABLE_BUILD=ON ..
make -j$(nproc)
sudo make install
```

### Build Options
- **`-DAVC_PORTABLE_BUILD=ON`** - Disable native CPU optimizations for better portability
- **`-DCMAKE_INSTALL_PREFIX=/custom/path`** - Custom installation directory

### Quick Sanity Check
```bash
./avc version   # Should print version and detected thread-count
```

---

## 4. Debug Builds

For development enable sanitizers and extra warnings:
```bash
mkdir build-debug && cd build-debug
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```

### Testing AGCL (Git Compatibility)
```bash
# After building, test AGCL functionality
./avc init
echo "test" > test.txt
./avc add test.txt
./avc commit -m "Test commit"
./avc agcl git-init
./avc agcl sync-to-git
./avc agcl verify-git
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
* **`march=native` errors on ARM** – use portable build: `cmake -DAVC_PORTABLE_BUILD=ON ..`
* **AGCL sync fails** – ensure you have both AVC and Git repositories initialized
* **GitHub 404 after push** – run `avc agcl verify-git` to check Git object validity

If you encounter problems that are not covered here, please open a GitHub issue. Pull requests with fixes are very welcome!
