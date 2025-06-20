# AVC - Archive Version Control

Welcome to **AVC** (Archive Version Control), a lightweight, Git-inspired version control system written in C. AVC is designed for simplicity, speed, and learning. This README will walk you through setup, usage, and all available features interactively!

---

## 🚀 Quick Start

```sh
# Clone the repository
$ git clone https://github.com/assembler-0/AVC.git
$ cd AVC-ArchiveVersionControl

# Build the project (requires CMake, OpenSSL, zlib)
$ mkdir build && cd build
$ cmake ..
$ make

# The 'avc' executable will be in the build directory
$ ./avc
```

---

## 🛠️ Setup Instructions

1. **Dependencies:**
   - C compiler (GCC/Clang)
   - CMake >= 3.20
   - OpenSSL
   - zlib

2. **Build:**
   ```sh
   mkdir build && cd build
   cmake ..
   make
   ```
   The `avc` binary will be created in the `build` directory.

3. **(Optional) Install:**
   ```sh
   sudo make install
   # Now you can run 'avc' from anywhere
   ```

---

## 🏁 Getting Started: Interactive Walkthrough

### 1. Initialize a New Repository
```sh
$ avc init
```
Creates a `.avc` directory in your project folder.

### 2. Add Files or Directories
```sh
$ avc add myfile.txt
$ avc add src/
```
You can add individual files or entire directories (recursively).

### 3. Check Status
```sh
$ avc status
```
Shows which files are staged for commit.

### 4. Commit Changes
```sh
$ avc commit -m "Initial commit"
# Or interactively:
$ avc commit
Enter a commit message (or use -m <msg>): My first commit
```

### 5. View Commit Log
```sh
$ avc log
```
See your commit history.

### 6. Remove Files
```sh
$ avc rm myfile.txt
$ avc rm --cached myfile.txt  # Only remove from staging
```

### 7. Reset to a Previous Commit
```sh
$ avc reset <commit-hash>
$ avc reset --hard <commit-hash>   # Restore working directory and index
$ avc reset --clean --hard <commit-hash>  # Wipe working directory (except .avc, .git, .idea) before restoring
```

---

## 📜 Available Commands & Flags

- `avc init`                : Initialize a new repository
- `avc add <file|dir>`      : Add file(s) or directory (recursively) to staging
- `avc status`              : Show staged files
- `avc commit [-m <msg>]`   : Commit staged changes (with message)
- `avc log`                 : Show commit history
- `avc rm [--cached] <file>`: Remove file from staging (and optionally from disk)
- `avc reset [--hard] [--clean] <commit-hash|HEAD|HEAD~1>` : Reset index/working directory

### Flags
- `--hard`   : Reset both index and working directory
- `--clean`  : Wipe working directory (except .avc, .git, .idea) before restoring
- `--cached` : Only remove from staging area (for `rm`)

---

## ✨ Features
- Fast, minimal, and easy to use
- SHA-256 based object storage (like Git)
- Add files or directories recursively
- Clean reset with safety prompt
- Human-friendly CLI messages
- Small binary size (16kB-36kB)
- Open source and hackable

---

## 💡 Tips & Best Practices
- Always check `avc status` before committing
- Use `avc log` to find commit hashes for reset
- Use `avc reset --clean --hard <hash>` for a truly clean restore
- Your `.avc` directory is your repo's brain—don't delete it!
- For help, just run `avc` with no arguments

---

## 🧑‍💻 Contributing & License
- Contributions welcome! Open an issue or PR.
- Licensed under the MIT License (see LICENSE file).

---

Happy versioning with AVC! 🎉
