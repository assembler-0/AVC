# AVC - Archive Version Control v0.2.0 "Cryo Falcon" - EXPERIMENTAL

[![License: GPL](https://img.shields.io/badge/License-GPL%203.0-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-0.2.0-brightgreen.svg)](https://github.com/assembler-0/AVC/releases)
[![Build Status](https://img.shields.io/badge/build-experimental-orange.svg)]()

**AVC** (Archive Version Control) is a high-performance, Git-inspired version control system written in C. Designed for speed, efficiency, and simplicity, AVC combines the familiar Git workflow with modern optimizations including multi-threaded operations and intelligent compression.

---

## 📚 Documentation

* **Build & Installation:** see [`BUILD.md`](docs/BUILD.md)
* **Usage Guide:** see [`USAGE.md`](docs/USAGE.md)
* **Contributing:** see [`CONTRIBUTING.md`](CONTRIBUTING)

---

## 🚀 Performance Highlights

### Benchmark Results (139MB Repository)
| Operation | AVC | Git | Performance |
|-----------|-----|-----|-------------|
| **Init** | 0.001s | 0.001s | ⚡ Equal |
| **Add** | 0.486s | 1.270s | ⚡ 2.6x faster |
| **Commit** | 0.053s | 0.244s | 🚀 **4.6x faster** |
| **Reset** | 0.323s | 0.497s | ⚡ 1.5x faster |
| **Repository Size** | 67MB | 96MB | 💾 **1.4x smaller** |

*Tested on Linux with OpenSSL 3.5 LTS, multi-threaded operations enabled*

## ✨ Key Features

### 🔧 Core Functionality
- **BLAKE3 content addressing** - Fast, secure, collision-resistant hashing
- **Git-like workflow** - Familiar commands and concepts
- **Multi-threaded operations** - Parallel processing for speed
- **Intelligent compression** - Smart libdeflate compression with size optimization
- **Directory support** - Full recursive directory operations
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
| Command | Description | Options |
|---------|-------------|---------|
| `avc init` | Initialize new repository | None |
| `avc add <path>` | Add files/directories to staging | None |
| `avc commit` | Commit staged changes | `-m <msg>`, `--no-compression` |
| `avc status` | Show repository status | None |
| `avc log` | Show commit history | None |
| `avc rm <path>` | Remove files/directories | `-r`, `--cached` |
| `avc reset <hash>` | Reset to commit | `--hard`, `--clean` |
| `avc clean` | Remove entire repository | None |
| `avc version` | Show version information | None |

### Flags Reference
- `-m <message>` - Commit message
- `--no-compression` - Disable compression for speed
- `-r` - Recursive directory operations
- `--cached` - Only remove from staging area
- `--hard` - Reset working directory
- `--clean` - Wipe working directory before reset

## 🏗️ Architecture

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
# Debug build with sanitizers
mkdir build-debug && cd build-debug
cmake -DCMAKE_BUILD_TYPE=Debug ..
make

# Run tests
make test
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

**AVC v0.1.5 "Arctic Fox"** - Fast, efficient, and reliable version control for the modern developer.

*Built with ❤️ by Atheria*
