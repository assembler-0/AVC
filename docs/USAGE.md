# USAGE GUIDE

This document covers the day-to-day workflow with **AVC** and **AGCL** (Git compatibility). For build instructions, see [`BUILD.md`](BUILD.md). For detailed AGCL usage, see [`AGCL_USAGE.md`](AGCL_USAGE.md).

---

## 1. Quick Start

### AVC Only (Local Development)
```bash
# Initialize repository
avc init

# Add files incrementally
avc add README.md
avc add src/main.c
# Or add all
avc add .

# Check status
avc status

# Commit changes
avc commit -m "Initial commit"
```

### AVC + GitHub (Full Workflow)
```bash
# Initialize both AVC and Git
avc init
avc agcl git-init

# Work with AVC
avc add .
avc commit -m "Initial commit"

# Convert to Git and push
avc agcl sync-to-git
git remote add origin https://github.com/user/repo.git
git push -u origin main
```

---

## 2. Basic Commands

### Core AVC Commands
| Command | Description |
|---------|-------------|
| `avc init` | Create a new repository (`.avc/`) |
| `avc add <path>` | Stage files / directories (incremental) |
| `avc status` | Show staged changes |
| `avc commit [-m <msg>]` | Commit staged changes |
| `avc log` | View commit history |
| `avc rm <path>` | Remove files (with `-r` for directories) |
| `avc reset <hash>` | Reset to a previous commit |
| `avc clean` | Delete the entire repository |
| `avc version` | Display version & build info |

### AGCL Commands (Git Compatibility)
| Command | Description |
|---------|-------------|
| `avc agcl git-init` | Initialize Git repo alongside AVC |
| `avc agcl sync-to-git` | Convert AVC objects to Git format |
| `avc agcl verify-git` | Verify Git repository state |

### Options Cheat-Sheet

```
-m <msg>         Commit message
-r               Recursive directory operations
--cached         Remove only from the index, keep working copy
--hard           Reset working tree as well
--clean          Wipe working tree before resetting
- --fast         Compression level 0 for speed
```

---

## 3. Advanced Usage

### Incremental File Management
```bash
# Add files one by one (properly supported)
avc add file1.txt
avc add file2.txt
avc add src/main.c

# Files accumulate in staging area
avc status  # Shows all staged files

# Commit all staged files
avc commit -m "Add multiple files"
```

### Large Projects
```bash
# Add entire project except build artifacts
avc add src/ include/ docs/
# Use --fast for speed on large projects
avc add --fast large-dataset/
```

### GitHub Integration
```bash
# After making commits in AVC
avc agcl sync-to-git

# Verify before pushing
avc agcl verify-git
git log --oneline  # Should show your commits

# Push to GitHub
git push origin main
```

### Reset Strategies
```bash
# Hard reset to a specific commit
avc reset --hard <hash>

# Clean reset (wipe working tree first)
avc reset --clean --hard <hash>
```

---

## 4. Tips & Tricks

* **Incremental Adding** – `avc add file1.txt file2.txt` works properly, files accumulate in staging
* **Multi-threading** – AVC automatically detects CPU cores for parallel processing
* **Git Compatibility** – Use AGCL to push AVC repos directly to GitHub/GitLab
* **Cross-platform** – Use `-DAVC_PORTABLE_BUILD=ON` for ARM/older CPUs
* **Compression** – zstd compression enabled by default for efficiency
* **Hardware** – SSDs and plenty of RAM noticeably speed up operations

---

## 5. Command Reference (Detailed)

### Common Workflows

**Local Development:**
```bash
avc init
avc add .
avc commit -m "Initial commit"
# Continue with normal AVC workflow
```

**GitHub Project:**
```bash
avc init && avc agcl git-init
avc add .
avc commit -m "Initial commit"
avc agcl sync-to-git
git remote add origin <repo-url>
git push -u origin main
```

**Incremental Updates:**
```bash
# Add new files
avc add new-feature.c
avc commit -m "Add new feature"

# Sync and push
avc agcl sync-to-git
git push origin main
```

For detailed AGCL usage and troubleshooting, see [`AGCL_USAGE.md`](AGCL_USAGE.md).
