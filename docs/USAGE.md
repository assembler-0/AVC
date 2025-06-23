# USAGE GUIDE

This document covers the day-to-day workflow with **AVC**.  If you are looking for build instructions, see [`BUILD.md`](BUILD.md).

---

## 1. Quick Start

```bash
# Initialise a repository in the current directory
avc init

# Stage files or directories
avc add README.md src/

# Inspect what is staged
avc status

# Commit your work
avc commit -m "Initial commit"
```

---

## 2. Basic Commands

| Command | Description |
|---------|-------------|
| `avc init` | Create a new repository (`.avc/`) |
| `avc add <path>` | Stage files / directories recursively |
| `avc status` | Show staged changes |
| `avc commit [-m <msg>]` | Commit staged changes |
| `avc log` | View commit history |
| `avc rm <path>` | Remove files (with `-r` for directories) |
| `avc reset <hash>` | Reset to a previous commit |
| `avc clean` | Delete the entire repository |
| `avc version` | Display version & build info |

### Options Cheat-Sheet

```
-m <msg>        Commit message
-r               Recursive directory operations
--cached         Remove only from the index, keep working copy
--hard           Reset working tree as well
--clean          Wipe working tree before resetting
```

---

## 3. Advanced Usage

### Performance-Oriented Commits
```bash
# Maximum speed – skips compression
avc commit --no-compression -m "Fast commit"
```

### Large Projects
```bash
# Add entire project except build artefacts (manual exclusion)
avc add src/ include/
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

* **Multi-threading** – AVC automatically detects CPU cores.  Ensure you have enough RAM when committing very large data sets.
* **Compression** – For archiving or storage-space sensitive workflows, keep compression enabled (default).
* **Hardware** – SSDs and plenty of RAM noticeably speed up operations.

---

## 5. Command Reference (Detailed)

See the built-in help for any command:
```bash
avc <command> --help
```

Complete flags and sub-commands are documented in the man-pages (coming soon).
