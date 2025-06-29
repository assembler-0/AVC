# ⚠️ IMPORTANT NOTES - AVC Behavior

## 🚨 CRITICAL: Partial Add Operations

**DANGER**: Adding individual files to an existing large repository will **REPLACE ALL FILES** with only the files you specify!

### ❌ Examples of Destructive Operations:
```bash
# DANGEROUS - Will replace 88K files with just 1 file!
avc add single_file.txt
avc commit -m "Added file"
git push  # Remote now only has 1 file!

# DANGEROUS - Will replace everything with just these 3 files!
avc add file1.txt file2.txt file3.txt
avc commit -m "Added files"
```

### ✅ Safe Operations:
```bash
# SAFE - Add all files (preserves existing + adds new)
avc add .
avc commit -m "Added new files"

# SAFE - Add specific directory (preserves existing + adds directory)
avc add src/
avc commit -m "Added src directory"
```

### 🔍 Why This Happens:
- AVC (like Git) commits **only what's in the staging area**
- `avc add file.txt` stages **ONLY** that file
- `avc commit` creates a commit with **ONLY** staged files
- Previous files are **not included** in the new commit
- `git push` replaces remote with the new commit (1 file)

### 🛠️ Recovery:
If you accidentally did this:
```bash
# Reset to previous commit (before the destructive add)
avc reset --hard HEAD~1

# Or restore from backup/remote
git pull origin main  # If you have a backup on remote
```

## 📝 Best Practices

### Always Use These Commands:
- `avc add .` - Add all files safely
- `avc add directory/` - Add entire directories
- `avc status` - Check what will be committed

### Avoid These Commands (Unless Intentional):
- `avc add single_file.txt` - Only if you want JUST this file
- `avc add *.txt` - Only if you want JUST these files

## 🎯 Quick Reference

| Command | Result | Safe? |
|---------|--------|-------|
| `avc add .` | Adds all files | ✅ SAFE |
| `avc add src/` | Adds src directory | ✅ SAFE |
| `avc add file.txt` | **ONLY file.txt in commit** | ⚠️ DANGEROUS |
| `avc add *.c` | **ONLY .c files in commit** | ⚠️ DANGEROUS |

## 💡 Pro Tips

1. **Always run `avc status`** before committing
2. **Use `avc add .`** as your default command
3. **Keep backups** of important repositories
4. **Test with small repos** before using on large projects
5. **Understand staging area** behavior before advanced usage

## 📁 Empty Directory Preservation

### .avckeep Files
AVC can preserve empty directories using `.avckeep` placeholder files:

```bash
# Preserve empty directories
avc add -e .
avc add --empty-dirs folder/
```

**What happens:**
- AVC detects empty directories during `avc add -e`
- Creates `.avckeep` files in empty directories
- These files contain comments explaining their purpose
- Empty directories are preserved in commits and Git pushes

**Notes:**
- `.avckeep` files are safe to delete if directory gets other files
- Similar to `.gitkeep` convention used by Git users
- Only created when `-e` or `--empty-dirs` flag is used
- Works with any path: `avc add -e .` or `avc add -e specific/folder/`

---

**Remember**: AVC follows Git's staging area model. Only staged files are included in commits!