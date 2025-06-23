# AVC - Archive Version Control v0.1.0 "Arctic Fox"

[![License: GPL](https://img.shields.io/badge/License-GPL-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-0.1.0-orange.svg)](https://github.com/assembler-0/AVC/releases)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)]()

**AVC** (Archive Version Control) is a high-performance, Git-inspired version control system written in C. Designed for speed, efficiency, and simplicity, AVC combines the familiar Git workflow with modern optimizations including multi-threaded operations and intelligent compression.

## 🚀 Performance Highlights

### Benchmark Results (70MB Repository)
| Operation | AVC | Git | Performance |
|-----------|-----|-----|-------------|
| **Init** | 0.001s | 0.001s | ⚡ Equal |
| **Add** | 1.260s | 1.270s | ⚡ 0.8% faster |
| **Commit** | 0.074s | 0.244s | 🚀 **3.3x faster** |
| **Reset** | 0.652s | 0.497s | ⚡ 31% slower |
| **Repository Size** | 70MB | 96MB | 💾 **27% smaller** |

*Tested on Linux with OpenSSL 3.5 LTS, multi-threaded operations enabled*

## ✨ Key Features

### 🔧 Core Functionality
- **SHA-256 content addressing** - Secure, collision-resistant hashing
- **Git-like workflow** - Familiar commands and concepts
- **Multi-threaded operations** - Parallel processing for speed
- **Intelligent compression** - Smart libdeflate compression with size optimization
- **Directory support** - Full recursive directory operations

### 🚀 Performance Optimizations
- **OpenMP parallelization** - Multi-threaded file processing
- **Fast compression** - Level 6 libdeflate with smart detection
- **Memory-efficient operations** - Streaming file processing
- **Optimized object storage** - Git-style subdirectory structure
- **Progress indicators** - Real-time operation feedback

### 🛡️ Advanced Features
- **Smart compression detection** - Avoids double-compressing already compressed files
- **Optional compression** - `--no-compression` flag for maximum speed
- **Directory removal** - `-r` flag for recursive directory operations
- **Timing information** - Performance metrics for all operations
- **Thread configuration** - Automatic CPU core detection and utilization

## 🛠️ Installation

### Prerequisites
- **C Compiler**: GCC 7+ or Clang 6+
- **CMake**: 3.20 or higher
- **OpenSSL**: 1.1.1 or higher
- **libdeflate**: 1.0 or higher
- **OpenMP**: For multi-threading support

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/assembler-0/AVC.git
cd AVC-ArchiveVersionControl

# Build the project
   mkdir build && cd build
   cmake ..
   make

# Optional: Install system-wide
sudo make install
```

### Quick Test
```bash
# Test the installation
./avc version
```

## 📖 Usage Guide

### Basic Workflow

#### 1. Initialize Repository
```bash
avc init
# Creates .avc directory with repository structure
```

#### 2. Add Files
```bash
# Add individual files
avc add file.txt

# Add entire directories (recursive)
avc add src/

# Add multiple items
avc add file1.txt file2.txt directory/
```

#### 3. Check Status
```bash
avc status
# Shows staged files and their status
```

#### 4. Commit Changes
```bash
# Quick commit with message
avc commit -m "Add new features"

# Interactive commit
avc commit
# Enter commit message when prompted

# Maximum speed (no compression)
avc commit --no-compression -m "Fast commit"
```

#### 5. View History
```bash
avc log
# Shows commit history with hashes and messages
```

#### 6. Remove Files
```bash
# Remove file from staging and working directory
avc rm file.txt

# Remove only from staging area
avc rm --cached file.txt

# Remove directory recursively
avc rm -r directory/
```

#### 7. Reset Operations
```bash
# Reset to specific commit
avc reset <commit-hash>

# Hard reset (restore working directory)
avc reset --hard <commit-hash>

# Clean reset (wipe working directory first)
avc reset --clean --hard <commit-hash>
```

### Advanced Usage

#### Performance Optimization
```bash
# Disable compression for maximum speed
avc commit --no-compression -m "Performance commit"

# Check version and thread count
avc version
```

#### Directory Operations
```bash
# Add entire project
avc add .

# Remove directory tree
avc rm -r build/

# Add with exclusions (manual)
avc add src/ include/ # Skip build/, .git/, etc.
```

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
- **SHA-256 hashing** for content addressing
- **Git-style subdirectories** (`.avc/objects/ab/cdef...`)
- **Intelligent compression** with fallback to uncompressed storage
- **Streaming operations** for memory efficiency

### Multi-threading
- **OpenMP parallelization** for file processing
- **Automatic thread detection** and configuration
- **Dynamic scheduling** for optimal load balancing
- **Thread-safe operations** throughout the codebase

### Compression Strategy
- **Level 6 libdeflate** for balanced speed/size
- **Smart detection** of already compressed files
- **Size thresholds** to skip compression for small files
- **Optional compression** via command-line flag

## 🧪 Testing

### Performance Testing
```bash
# Test with large repository
mkdir test-repo && cd test-repo
avc init
# Add large files/directories
avc add .
avc commit -m "Performance test"
avc version
```

### Feature Testing
```bash
# Test all commands
avc init
echo "test" > file.txt
avc add file.txt
avc commit -m "Test commit"
avc status
avc log
avc rm file.txt
avc reset --hard HEAD
```

## 📊 Performance Tips

### For Maximum Speed
1. **Use `--no-compression`** for large binary files
2. **Add files in batches** rather than individually
3. **Use SSD storage** for better I/O performance
4. **Ensure sufficient RAM** for parallel operations

### For Maximum Compression
1. **Enable compression** (default behavior)
2. **Focus on text files** for best compression ratios
3. **Avoid already compressed files** (PNG, JPEG, etc.)

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

**AVC v0.1.0 "Arctic Fox"** - Fast, efficient, and reliable version control for the modern developer.

*Built with  by Atheria*
