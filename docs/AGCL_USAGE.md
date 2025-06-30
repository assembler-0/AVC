# AGCL Usage Guide

**AGCL** (AVC Git Compatibility Layer) enables seamless integration between AVC and Git, allowing you to push AVC repositories directly to GitHub, GitLab, and other Git hosting services.

## Overview

AGCL converts between two formats:
- **AVC Format**: BLAKE3 hashes (64-char) + zstd compression
- **Git Format**: SHA-1 hashes (40-char) + zlib compression

## Complete Workflow

### 1. Initialize Repositories
```bash
# Initialize AVC repository
avc init

# Initialize Git repository alongside AVC
avc agcl git-init
```

### 2. Work with Files (AVC)
```bash
# Add files incrementally
avc add file1.txt
avc add file2.txt
# Or add all files
avc add .

# Commit changes
avc commit -m "Your commit message"

# Check status
avc status
```

### 3. Convert to Git Format
```bash
# Convert AVC objects to Git format
avc agcl sync-to-git

# Verify conversion (optional)
avc agcl verify-git
```

### 4. Push to GitHub
```bash
# Add remote repository
git remote add origin https://github.com/username/repo.git

# Push to GitHub
git push -u origin main
```

## AGCL Commands

### `avc agcl git-init`
Initializes a Git repository alongside your AVC repository.

**What it creates:**
- `.git/` directory structure
- `.git/HEAD` pointing to `refs/heads/main`
- `.git/config` with proper Git configuration
- `.git/objects/` for Git objects
- `.git/refs/` for Git references

### `avc agcl sync-to-git`
Converts AVC objects (commits, trees, blobs) to Git format.

**Conversion process:**
1. **Commits**: Converts BLAKE3 commit hashes to SHA-1
2. **Trees**: Converts directory structures with proper Git tree format
3. **Blobs**: Converts file contents with SHA-1 hashing
4. **References**: Updates `.git/refs/heads/main` with latest commit

**Hash mapping:**
- Maintains a mapping file (`.git/avc-map`) between AVC and Git hashes
- Ensures consistent conversion across multiple syncs

### `avc agcl verify-git`
Verifies that the Git repository is in a valid state.

**Checks performed:**
- Git objects exist and are accessible
- Commit references are valid
- Repository structure is complete
- Objects pass `git fsck` validation

## Troubleshooting

### Issue: Empty commits after sync
**Cause**: Commit message parsing failed
**Solution**: Ensure commit messages are properly formatted

### Issue: Git shows files as deleted
**Cause**: Working directory and Git index are out of sync
**Solution**: This is normal after AGCL sync - the Git repository contains the objects but working directory shows AVC files

### Issue: GitHub shows 404 after push
**Cause**: Invalid Git objects or empty repository
**Solution**: 
1. Run `avc agcl verify-git`
2. Check `git log --oneline` shows commits
3. Ensure commit messages are not empty

### Issue: ARM/cross-platform build fails
**Cause**: Native optimizations not supported
**Solution**: Use portable build:
```bash
cmake -DAVC_PORTABLE_BUILD=ON ..
```

## Advanced Usage

### Multiple Commits
```bash
# Work normally with AVC
avc add file1.txt
avc commit -m "Add file1"

avc add file2.txt  
avc commit -m "Add file2"

# Sync all commits at once
avc agcl sync-to-git

# Push entire history
git push origin main
```

### Incremental Sync
AGCL is designed to handle incremental syncs efficiently:
- Only new commits are converted
- Existing Git objects are reused
- Hash mapping prevents duplicate work

### Repository Maintenance
```bash
# Clean up AVC repository
avc clean

# Remove Git repository
rm -rf .git

# Start fresh
avc agcl git-init
```

## Technical Details

### Hash Conversion
- **AVC → Git**: BLAKE3 (64-char) converted to SHA-1 (40-char)
- **Mapping**: Stored in `.git/avc-map` for consistency
- **Collision Handling**: Uses cryptographic hashing to prevent conflicts

### Object Format Conversion
- **Blobs**: Direct content conversion with new hash
- **Trees**: Binary format conversion with proper Git tree structure
- **Commits**: Header parsing and timestamp conversion

### Compression Conversion
- **AVC**: zstd compression
- **Git**: zlib compression
- **Automatic**: AGCL handles compression format differences

## Best Practices

1. **Always verify**: Run `avc agcl verify-git` before pushing
2. **Incremental workflow**: Add files individually for better tracking
3. **Meaningful commits**: Use descriptive commit messages
4. **Regular sync**: Sync to Git format regularly to catch issues early
5. **Backup important work**: AGCL is stable but backup critical repositories

## Limitations

- **One-way sync**: Currently only AVC → Git (Git → AVC planned)
- **Hash mapping**: Requires mapping file for consistency
- **Performance**: Large repositories may take time to convert initially
- **Git features**: Some advanced Git features not supported in reverse direction

## Examples

### Basic Project
```bash
# Setup
avc init
avc agcl git-init

# Work
echo "Hello World" > hello.txt
avc add hello.txt
avc commit -m "Initial commit"

# Publish
avc agcl sync-to-git
git remote add origin https://github.com/user/project.git
git push -u origin main
```

### Multi-file Project
```bash
# Setup
avc init
avc agcl git-init

# Add files incrementally
avc add README.md
avc add src/main.c
avc add Makefile.legacy
avc commit -m "Add project files"

# More changes
avc add tests/test.c
avc commit -m "Add tests"

# Publish all
avc agcl sync-to-git
git push origin main
```

AGCL makes AVC the first version control system with native GitHub compatibility while maintaining the performance benefits of BLAKE3 and zstd compression.