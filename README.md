# AVC - Archive Version Control v0.3.0 "Git Bridge" - STABLE

[![License: GPL](https://img.shields.io/badge/License-GPL%203.0-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-0.3.0-brightgreen.svg)](https://github.com/assembler-0/AVC/releases)
[![Build Status](https://img.shields.io/badge/build-stable-green.svg)]()

**AVC** (Archive Version Control) is a high-performance version control system with **Git compatibility** through AGCL (AVC Git Compatibility Layer). Push AVC repositories directly to GitHub while enjoying BLAKE3 hashing, libdeflate compression, and multi-threaded operations.

---

## 📚 Documentation

* **Build & Installation:** see [`BUILD.md`](docs/BUILD.md)
* **Usage Guide:** see [`USAGE.md`](docs/USAGE.md)
* **AGCL Guide:** see [`AGCL_USAGE.md`](docs/AGCL_USAGE.md)
* **Contributing:** see [`CONTRIBUTING.md`](CONTRIBUTING)

---

## 🚀 Performance Highlights

### Benchmark Results (139MB Repository)
| Operation | AVC  | AVC --fast | Git  | Performance               |
|-----------|------|------------|------|---------------------------|
| **Init** | 0.001s | 0.001      | 0.001s | ⚡ Equal                   |
| **Add** | 0.4s | 0.1s       | 1.1s | ⚡ Up to **12.7x** faster  |
| **Commit** | 0.053s | 0.006s     | 0.244s | 🚀 **Up to 40.7x faster** |
| **Reset** | 0.323s | 0.317s     | 0.497s | ⚡ 1.5x faster             |
| **Repository Size** | 67MB | 140MB      | 96MB | 💾 **Up to 1.4x smaller** |

**Best run*

*Tested on Linux with OpenSSL 3.5 LTS, multi-threaded operations enabled*

## ✨ Key Features

### 🔧 Core Functionality
- **Git Compatibility (AGCL)** - Push AVC repos directly to GitHub/GitLab
- **BLAKE3 content addressing** - Fast, secure, collision-resistant hashing
- **Incremental staging** - `avc add file1.txt file2.txt` works properly
- **Multi-threaded operations** - Parallel processing for speed
- **Intelligent compression** - Smart libdeflate compression with size optimization
- **Cross-platform builds** - ARM/portable build support
- **Large file support** - Handles files up to 1GB with optimized memory management

### 🚀 Performance Optimizations
- **OpenMP parallelization** - Multi-threaded file processing
- **Fast compression** - Level 6 libdeflate with smart detection
- **Memory-efficient operations** - Streaming file processing
- **Optimized object storage** - Git-style subdirectory structure
- **Progress indicators** - Real-time operation feedback
- **Memory pool optimization** - Efficient memory allocation for large files

### 🛡️ Advanced Features
- **Smart compression detection** - Avoids double-compressing already compressed files
- **Consistent compression** - Pure libdeflate compression with no fallbacks
- **Directory removal** - `-r` flag for recursive directory operations
- **Thread configuration** - Automatic CPU core detection and utilization
- **Repository cleanup** - `clean` command to remove entire repository
- **Robust error handling** - Detailed error messages and recovery
- **Large file optimization** - 1MB chunks and 1GB file size support

## 🔧 Command Reference

### Core Commands
| Command | Description | Options            |
|---------|-------------|--------------------|
| `avc init` | Initialize new repository | None               |
| `avc add <path>` | Add files/directories to staging | `f`, `--fast`      |
| `avc commit` | Commit staged changes | `-m <msg>`         |
| `avc status` | Show repository status | None               |
| `avc log` | Show commit history | None               |
| `avc rm <path>` | Remove files/directories | `-r`, `--cached`   |
| `avc reset <hash>` | Reset to commit | `--hard`, `--clean` |
| `avc clean` | Remove entire repository | None               |
| `avc version` | Show version information | None               |

### AGCL Commands (Git Compatibility)
| Command | Description | Purpose            |
|---------|-------------|--------------------|
| `avc agcl git-init` | Initialize Git repo alongside AVC | Setup Git compatibility |
| `avc agcl sync-to-git` | Convert AVC objects to Git format | Prepare for GitHub push |
| `avc agcl verify-git` | Verify Git repository state | Debug conversion issues |

### Flags Reference
- `-m <message>` - Commit message
- `-r` - Recursive directory operations
- `--cached` - Only remove from staging area
- `--hard` - Reset working directory
- `--clean` - Wipe working directory before reset
- `--fast` - Compression level 0 for speed

## 🏗️ Architecture

### Dual Format Support
- **AVC Format**: BLAKE3 hashes (64-char) + libdeflate compression
- **Git Format**: SHA-1 hashes (40-char) + zlib compression  
- **AGCL Bridge**: Converts between formats seamlessly
- **GitHub Compatible**: Push AVC repos to any Git hosting service

### Object Storage
- **BLAKE3 hashing** for fast, secure content addressing
- **Git-style subdirectories** (`.avc/objects/ab/cdef...`)
- **Pure libdeflate compression** with consistent compressed storage
- **Streaming operations** for memory efficiency
- **Large file optimization** with 1MB chunks and 1GB file support

### Multi-threading
- **OpenMP parallelization** for file processing
- **Automatic thread detection** and configuration
- **Dynamic scheduling** for optimal load balancing
- **Thread-safe operations** throughout the codebase

### Compression Strategy
- **Pure libdeflate compression** for consistent performance
- **No fallback compression** - eliminates compatibility issues
- **Large buffer support** - 1MB compression buffers for big files
- **Memory pool optimization** - Efficient allocation for large operations


## 📊 Performance Tips

### For Maximum Speed
1. **Use multi-threaded operations** - Automatic CPU core utilization
2. **Add files in batches** rather than individually
3. **Use SSD storage** for better I/O performance
4. **Ensure sufficient RAM** for parallel operations
5. **Large files are optimized** - 1MB chunks and 1GB file support

### For Maximum Compression
1. **Consistent compression** - Pure libdeflate for reliability
2. **Focus on text files** for best compression ratios
3. **Large file handling** - Optimized for files up to 1GB

## 🤝 Contributing

We welcome contributions! Please see our contributing guidelines:

1. **Fork the repository**
2. **Create a feature branch**
3. **Make your changes**
4. **Add tests if applicable**
5. **Submit a pull request**

### Development Setup
```bash
# Standard build (native optimizations)
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# Portable build (ARM/cross-platform)
cmake -DCMAKE_BUILD_TYPE=Release -DAVC_PORTABLE_BUILD=ON ..
make -j$(nproc)

# Debug build with sanitizers
mkdir build-debug && cd build-debug
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```

## 📄 License

This project is licensed under the **GPL License** - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- **Git** - For inspiration and workflow concepts
- **OpenSSL** - For cryptographic functions
- **libdeflate** - For compression capabilities
- **OpenMP** - For parallel processing support

## 📞 Support

- **Issues**: [GitHub Issues](https://github.com/assembler-0/AVC/issues)
- **Discussions**: [GitHub Discussions](https://github.com/assembler-0/AVC/discussions)
- **Documentation**: This README and inline code comments

---

**AVC v0.3.0 "Git Bridge"** - The first version control system with native GitHub compatibility.

*Built with ❤️ by Atheria*
