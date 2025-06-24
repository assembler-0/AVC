# AVC Git Compatibility Layer (AGCL)

The AVC Git Compatibility Layer allows you to use AVC for fast local operations while maintaining Git compatibility for remote operations and collaboration.

## 🚀 Quick Start

### 1. Initialize Git alongside AVC
```bash
# Initialize AVC repository
avc init

# Initialize Git repository alongside AVC
avc agcl git-init
```

### 2. Work with AVC (fast local operations)
```bash
# Add files (fast with AVC)
avc add .

# Commit (fast with AVC)
avc commit -m "Initial commit"

# Check status (fast with AVC)
avc status
```

### 3. Sync to Git for remote operations
```bash
# Sync AVC objects to Git format
avc agcl sync-to-git

# Now you can use Git commands
git push origin main
git pull origin main
```

## 🔄 Workflow

### Local Development (AVC)
```bash
avc add <files>      # Fast staging
avc commit -m "msg"  # Fast commits
avc status          # Fast status
avc log             # Fast history
```

### Remote Operations (Git)
```bash
avc agcl sync-to-git  # Convert AVC → Git
git push origin main  # Push to remote
git pull origin main  # Pull from remote
```

## 📋 Commands

### `avc agcl git-init`
Initializes a Git repository alongside your AVC repository.

**Creates:**
- `.git/` directory structure
- `.git/config` with basic configuration
- `.git/HEAD` pointing to `refs/heads/main`

### `avc agcl sync-to-git`
Converts AVC objects to Git format and updates Git HEAD.

**Converts:**
- AVC blobs → Git blobs (BLAKE3 → SHA-1)
- AVC trees → Git trees (with converted hashes)
- AVC commits → Git commits (with converted hashes)

### `avc agcl sync-from-git` (Coming Soon)
Converts Git objects to AVC format.

### `avc agcl migrate` (Coming Soon)
Converts an existing Git repository to AVC format.

## 🔧 Technical Details

### Object Format Compatibility
Both AVC and Git use the same object format:
```
"type size\0content"
```

### Hash Conversion
- **AVC**: BLAKE3 (64 characters)
- **Git**: SHA-1 (40 characters)
- **Conversion**: BLAKE3 hash → SHA-1 hash mapping

### Compression
- **AVC**: libdeflate (fast, efficient)
- **Git**: zlib (standard, compatible)
- **Conversion**: Automatic compression format conversion

### Storage Structure
```
.avc/objects/ab/cdef...  # AVC objects (BLAKE3)
.git/objects/ab/cdef...  # Git objects (SHA-1)
```

## 🎯 Use Cases

### 1. **Fast Local Development**
Use AVC for all local operations:
- Faster commits and status checks
- Better compression with libdeflate
- Optimized for large repositories

### 2. **Git Ecosystem Integration**
Use Git for remote operations:
- GitHub, GitLab, Bitbucket compatibility
- Standard Git tools and workflows
- Team collaboration

### 3. **Migration Path**
- Convert existing Git repos to AVC
- Maintain Git compatibility
- Gradual migration strategy

## 🔮 Future Features

### Planned Commands
- `avc agcl sync-from-git` - Sync Git → AVC
- `avc agcl migrate` - Convert Git repo to AVC
- `avc agcl export` - Export AVC repo as Git
- `avc agcl import` - Import Git repo to AVC

### Advanced Features
- **Bidirectional sync** - Automatic AVC ↔ Git conversion
- **Hash mapping table** - Persistent BLAKE3 ↔ SHA-1 mapping
- **Incremental sync** - Only sync changed objects
- **Conflict resolution** - Handle divergent histories

## 🛠️ Implementation Notes

### Hash Conversion Strategy
The current implementation uses a simple approach:
1. **BLAKE3 → SHA-1**: Hash the BLAKE3 hash with SHA-1
2. **SHA-1 → BLAKE3**: Hash the SHA-1 hash with BLAKE3

For production use, consider:
- **Persistent mapping table** for consistent conversions
- **Content-based mapping** for better deduplication
- **Collision detection** and resolution

### Performance Considerations
- **Lazy conversion**: Only convert when needed
- **Caching**: Cache conversion results
- **Batch operations**: Convert multiple objects at once

## 🚨 Limitations

### Current Limitations
1. **Hash conversion** is not perfect (potential collisions)
2. **No bidirectional sync** yet
3. **No conflict resolution** for divergent histories
4. **Manual sync** required (not automatic)

### Workarounds
1. **Use AVC for local work** - avoid frequent conversions
2. **Sync before Git operations** - ensure compatibility
3. **Test thoroughly** - verify conversions work correctly

## 📞 Support

For issues with AGCL:
1. Check that both AVC and Git repos are initialized
2. Verify object format compatibility
3. Test with simple repositories first
4. Report issues with conversion examples

---

**AGCL** - The bridge between AVC's speed and Git's ecosystem! 🚀 