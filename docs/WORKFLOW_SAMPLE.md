# AVC + Git Collaboration Workflow

This document shows how to use AVC with Git hosting services like GitHub/GitLab while collaborating with Git users.

## 🚀 Quick Start: Migrate Existing Git Project

```bash
# One-command migration from any Git repository
avc agcl migrate https://github.com/user/awesome-project.git
cd awesome-project/

# Ready to push AVC-powered version
git push -f origin main
```

## 🔄 Daily Workflow: AVC + Git Collaboration

### **Scenario**: You use AVC, teammates use Git

```bash
# 1. Pull changes from Git teammates
git pull origin main

# 2. Import Git changes to AVC format
avc agcl sync-from-git

# 3. Work with AVC (faster than Git!)
avc add new-feature.c
avc commit -m "Added awesome feature"

# 4. Export AVC changes to Git format  
avc agcl sync-to-git

# 5. Push to Git remote for teammates
git push origin main
```

## 📋 Complete Workflow Examples

### **Example 1: Start Fresh AVC Project**

```bash
# Initialize new AVC project
mkdir my-project && cd my-project
avc init

# Add files and commit
echo "Hello AVC!" > README.md
avc add .
avc commit -m "Initial commit"

# Setup Git compatibility
avc agcl git-init
avc agcl sync-to-git

# Push to GitHub
git remote add origin https://github.com/user/my-project.git
git push -u origin main
```

### **Example 2: Migrate Existing Git Project**

```bash
# Clone and convert in one step
avc agcl migrate https://github.com/user/existing-project.git
cd existing-project/

# Project is now AVC-powered but Git-compatible
git push -f origin main  # Force push new AVC history
```

### **Example 3: Mixed Team Collaboration**

```bash
# Morning: Get teammate's Git changes
git pull origin main
avc agcl sync-from-git

# Work with AVC (enjoy the speed!)
avc add src/
avc commit -m "Refactored core module"
avc add tests/
avc commit -m "Added comprehensive tests"

# Afternoon: Share with Git teammates  
avc agcl sync-to-git
git push origin main
```

## 🔧 Troubleshooting

### **Files Disappeared After Sync**
```bash
# This should not happen anymore, but if it does:
avc reset --hard HEAD
```

### **Sync Conflicts**
```bash
# Check repository state
avc status
avc agcl verify-git

# Reset if needed
avc reset --hard HEAD
```

### **Git Push Rejected**
```bash
# Force push after migration (expected)
git push -f origin main

# Or create new branch
git checkout -b avc-migration
git push origin avc-migration
```

## 🎯 Best Practices

### **For AVC Users**
1. **Always sync before/after Git operations**:
   ```bash
   git pull && avc agcl sync-from-git  # Before work
   avc agcl sync-to-git && git push    # After work
   ```

2. **Use AVC for local development**:
   ```bash
   avc add .        # Fast staging
   avc commit -m "" # Fast commits
   avc status       # Instant status
   ```

3. **Batch Git operations**:
   ```bash
   # Work with AVC locally
   avc add file1.c && avc commit -m "Feature A"
   avc add file2.c && avc commit -m "Feature B"
   
   # Sync to Git once
   avc agcl sync-to-git && git push
   ```

### **For Mixed Teams**
1. **Establish sync points**: Agree when to sync (daily, per feature, etc.)
2. **Use descriptive commit messages**: Help Git users understand AVC commits
3. **Document the workflow**: Share this guide with your team


## 🎉 Result

**Best of both worlds**: AVC performance + Git ecosystem! 

Your team gets faster local development while maintaining full compatibility with Git hosting and collaboration tools.

---

**Happy coding with AVC!** 🚀