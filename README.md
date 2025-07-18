# AVC - Archive Version Control v0.4.0 "Velocity" 
[![License: GPL](https://img.shields.io/badge/License-GPL%203.0-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-0.4.0-brightgreen.svg)](https://github.com/assembler-0/AVC/releases)
[![Build Status](https://img.shields.io/badge/build-stable-greeen.svg)]()

**AVC** (Archive Version Control) is a high-performance version control system with **full bidirectional Git compatibility** (kinda) through AGCL (AVC Git Compatibility Layer). Migrate existing Git repositories, collaborate with Git users, and push to GitHub/GitLab while enjoying BLAKE3 hashing, zstd compression, and multi-threaded operations.

---

## 📚 Documentation

* **Build & Installation:** see [`BUILD.md`](docs/BUILD.md)
* **Usage Guide:** see [`USAGE.md`](docs/USAGE.md)
* **AGCL Guide:** see [`AGCL_USAGE.md`](docs/AGCL_USAGE.md)
* **Git Collaboration:** see [`WORKFLOW_SAMPLE.md`](docs/WORKFLOW_SAMPLE.md)
* **Contributing:** see [`CONTRIBUTING.md`](CONTRIBUTING)
* **Notes:** see [`NOTES.md`](docs/IMPORTANT_NOTES.md)

---

## 🚀 Performance Highlights

### Benchmark Results (95000+ files)
| Operation                        | AVC    | Git     | Performance               |
|----------------------------------|--------|---------|---------------------------|
| **Init**                         | 0.001  | 0.001s  | ⚡ Equal                   |
| **Add**                          | 1.621s | 11.358s | ⚡ Up to **7x** faster     |
| **Commit**                       | 0.370s | 5.766s  | 🚀 **Up to 15.6x faster** |
| **Reset****                      | 0.854s | 3.236s  | ⚡ 3.8x faster             |
| **Size (pre-commit)** | 521MB  | 634MB   | 💾 **Up to 1.2x smaller** |

**Best run*

***Directory is cleaned for each run*

*Tested on Linux with Kernel 6.15.3 source with Intel Core i3-12100F*

## ✨ Key Features

### 🔧 Core Functionality
- **Bidirectional Git Compatibility (AGCL)** - Full sync between AVC and Git formats
- **One-command migration** - Convert any Git repo to AVC instantly
- **Mixed team collaboration** - AVC users work with Git users seamlessly
- **BLAKE3 content addressing** - Fast, secure, collision-resistant hashing
- **Incremental staging** - `avc add file1.txt file2.txt` works properly
- **Hierarchical tree structure** - Preserves directory hierarchy (unlike flat storage)
- **Empty directory support** - Optional preservation with `-e` flag
- **.avcignore support** - Skip files/directories using ignore patterns
- **Multi-threaded operations** - Parallel processing for speed
- **Intelligent compression** - Smart zstd compression with size optimization
- **Cross-platform builds** - ARM/portable build support
- **Large file support** - Handles files up to 1GB with optimized memory management

### 🚀 Performance Optimizations
- **OpenMP parallelization** - Multi-threaded file processing
- **Fast compression** - Level 6 zstd with smart detection
- **Memory-efficient operations** - Streaming file processing
- **Optimized object storage** - Git-style subdirectory structure
- **Progress indicators** - Real-time operation feedback
- **Memory pool optimization** - Efficient memory allocation for large files

### 🛡️ Advanced Features
- **Smart compression detection** - Avoids double-compressing already compressed files
- **Consistent compression** - Pure zstd compression with no fallbacks
- **Directory removal** - `-r` flag for recursive directory operations
- **Thread configuration** - Automatic CPU core detection and utilization
- **Repository cleanup** - `clean` command to remove entire repository
- **Robust error handling** - Detailed error messages and recovery
- **Large file optimization** - 1MB chunks and 1GB file size support

## 📖 Usage Examples

### Basic Operations
```bash
# Initialize repository
avc init

# Add files (basic)
avc add file.txt
avc add folder/

# Add all files in current directory (respects .avcignore)
avc add .

# Add with empty directory preservation
avc add -e .
avc add --empty-dirs folder/

# Add with fast compression
avc add -f large_file.zip

# Combine flags
avc add -ef .  # Fast compression + preserve empty dirs

# Commit changes
avc commit -m "Your commit message"
```

### Git Compatibility (AGCL)
```bash
# Setup Git compatibility
avc agcl git-init

# Push to GitHub/GitLab
avc agcl push

# Pull from remote
avc agcl pull

# Migrate existing Git repo
avc agcl migrate https://github.com/user/repo.git
```

## 🔧 Command Reference

### Core Commands
| Command | Description | Options            |
|---------|-------------|--------------------|
| `avc init` | Initialize new repository | None               |
| `avc add <path>` | Add files/directories to staging | `-f`, `--fast`, `-e`, `--empty-dirs` |
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
| `avc agcl migrate <url>` | Clone and convert Git repo to AVC | One-command migration |
| `avc agcl git-init` | Initialize Git repo alongside AVC | Setup Git compatibility |
| `avc agcl push` | Sync AVC to Git and push to origin | Share AVC changes with Git users |
| `avc agcl pull` | Pull from Git and add to AVC | Get Git changes into AVC |
| `avc agcl sync-to-git` | Convert AVC objects to Git format | Manual sync (advanced) |
| `avc agcl verify-git` | Verify Git repository state | Debug conversion issues |

### Flags Reference
- `-m <message>` - Commit message
- `-r` - Recursive directory operations
- `--cached` - Only remove from staging area
- `--hard` - Reset working directory
- `--clean` - Wipe working directory before reset
- `-f`, `--fast` - Use fast compression for speed
- `-e`, `--empty-dirs` - Preserve empty directories (creates .avckeep files)

### .avcignore File
Create a `.avcignore` file to exclude files/directories:
```
# Comments start with #
*.log
temp/
build/
node_modules/
.DS_Store
```

## 🏗️ Architecture

### Dual Format Support
- **AVC Format**: BLAKE3 hashes (64-char) + zstd compression
- **Git Format**: SHA-1 hashes (40-char) + zlib compression
- **AGCL Bridge**: Converts between formats seamlessly
- **GitHub Compatible**: Push AVC repos to any Git hosting service

### Object Storage
- **BLAKE3 hashing** for fast, secure content addressing
- **Git-style subdirectories** (`.avc/objects/ab/cdef...`)
- **Pure zstd compression** with consistent compressed storage
- **Streaming operations** for memory efficiency
- **Large file optimization** with 1MB chunks and 1GB file support

### Multi-threading
- **OpenMP parallelization** for file processing
- **Automatic thread detection** and configuration
- **Dynamic scheduling** for optimal load balancing
- **Thread-safe operations** throughout the codebase

### Compression Strategy
- **Pure zstd compression** for consistent performance
- **No fallback compression** - eliminates compatibility issues
- **Large buffer support** - 1MB compression buffers for big files
- **Memory pool optimization** - Efficient allocation for large operations

## 📄 License

This project is licensed under the **GPL License** - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- **Git** - For inspiration and workflow concepts
- **OpenSSL** - For cryptographic functions
- **zstd** - For compression capabilities
- **OpenMP** - For parallel processing support

## 📞 Support

- **Issues**: [GitHub Issues](https://github.com/assembler-0/AVC/issues)
- **Discussions**: [GitHub Discussions](https://github.com/assembler-0/AVC/discussions)
- **Documentation**: This README and inline code comments

---

**AVC v0.4.0 "Velocity"** - The first version control system with bidirectional Git compatibility and seamless team collaboration.

*Built with ❤️ by Atheria*
