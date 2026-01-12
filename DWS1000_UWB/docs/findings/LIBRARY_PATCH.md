# DW1000 Library Interrupt Bug - Patch Distribution Guide

This document provides ready-to-use patch files and instructions for applying the critical interrupt bug fix to the arduino-dw1000 library.

---

## Table of Contents

1. [Quick Apply](#quick-apply)
2. [Patch File Formats](#patch-file-formats)
3. [Application Methods](#application-methods)
4. [Verification](#verification)
5. [Distribution Instructions](#distribution-instructions)
6. [Submitting to Maintainers](#submitting-to-maintainers)
7. [Alternative: Using Fixed Fork](#alternative-using-fixed-fork)

---

## Quick Apply

### For UNIX/Linux/Mac

```bash
# Navigate to library source directory
cd lib/DW1000/src/

# Apply patch
patch -p0 < ../../../docs/findings/interrupt_bug_fix.patch

# Verify
grep -A 4 "interruptOnReceiveFailed" DW1000.cpp | grep LEN_SYS_MASK

# Should show 4 lines with LEN_SYS_MASK
```

### For Windows

Download the patch file and apply manually (see [Manual Application](#manual-application) below) or use Git Bash / WSL with the Linux commands above.

---

## Patch File Formats

### Format 1: Unified Diff (Standard)

**File**: `interrupt_bug_fix.patch`

```diff
--- DW1000.cpp.orig	2026-01-11 00:00:00.000000000 +0000
+++ DW1000.cpp	2026-01-11 00:00:00.000000000 +0000
@@ -990,10 +990,10 @@
 }

 void DW1000Class::interruptOnReceiveFailed(boolean val) {
-	setBit(_sysmask, LEN_SYS_STATUS, LDEERR_BIT, val);
-	setBit(_sysmask, LEN_SYS_STATUS, RXFCE_BIT, val);
-	setBit(_sysmask, LEN_SYS_STATUS, RXPHE_BIT, val);
-	setBit(_sysmask, LEN_SYS_STATUS, RXRFSL_BIT, val);
+	setBit(_sysmask, LEN_SYS_MASK, LDEERR_BIT, val);
+	setBit(_sysmask, LEN_SYS_MASK, RXFCE_BIT, val);
+	setBit(_sysmask, LEN_SYS_MASK, RXPHE_BIT, val);
+	setBit(_sysmask, LEN_SYS_MASK, RXRFSL_BIT, val);
 }

 void DW1000Class::interruptOnReceiveTimeout(boolean val) {
```

**Usage**:
```bash
cd lib/DW1000/src/
patch -p0 < path/to/interrupt_bug_fix.patch
```

---

### Format 2: Git Patch

**File**: `0001-fix-interrupt-buffer-overrun.patch`

```
From: Your Name <your.email@example.com>
Date: Sat, 11 Jan 2026 12:00:00 +0000
Subject: [PATCH] Fix critical buffer overrun in interruptOnReceiveFailed()

The function was using LEN_SYS_STATUS (5 bytes) instead of LEN_SYS_MASK
(4 bytes) when manipulating the _sysmask buffer. This caused a buffer
overrun that corrupted the interrupt mask register, preventing all
hardware interrupts from functioning.

This bug affected all code using setDefaults(), including BasicSender,
BasicReceiver, and all DW1000Ranging examples.

Fix: Change LEN_SYS_STATUS to LEN_SYS_MASK in all 4 setBit() calls in
interruptOnReceiveFailed().

Impact:
- Severity: CRITICAL
- Affects: All interrupt-based operations
- Testing: Verified on Arduino Uno with DW1000 modules
- Result: All examples now work correctly

---
 src/DW1000.cpp | 8 ++++----
 1 file changed, 4 insertions(+), 4 deletions(-)

diff --git a/src/DW1000.cpp b/src/DW1000.cpp
index 1234567..89abcdef 100644
--- a/src/DW1000.cpp
+++ b/src/DW1000.cpp
@@ -990,10 +990,10 @@ void DW1000Class::interruptOnReceived(boolean val) {
 }

 void DW1000Class::interruptOnReceiveFailed(boolean val) {
-	setBit(_sysmask, LEN_SYS_STATUS, LDEERR_BIT, val);
-	setBit(_sysmask, LEN_SYS_STATUS, RXFCE_BIT, val);
-	setBit(_sysmask, LEN_SYS_STATUS, RXPHE_BIT, val);
-	setBit(_sysmask, LEN_SYS_STATUS, RXRFSL_BIT, val);
+	setBit(_sysmask, LEN_SYS_MASK, LDEERR_BIT, val);
+	setBit(_sysmask, LEN_SYS_MASK, RXFCE_BIT, val);
+	setBit(_sysmask, LEN_SYS_MASK, RXPHE_BIT, val);
+	setBit(_sysmask, LEN_SYS_MASK, RXRFSL_BIT, val);
 }

 void DW1000Class::interruptOnReceiveTimeout(boolean val) {
--
2.30.0
```

**Usage**:
```bash
cd lib/DW1000/
git am < path/to/0001-fix-interrupt-buffer-overrun.patch
```

---

### Format 3: Manual Instructions (for any platform)

**File**: `MANUAL_FIX_INSTRUCTIONS.txt`

```
DW1000 Library Interrupt Bug Fix - Manual Instructions
=======================================================

File to edit: lib/DW1000/src/DW1000.cpp
Function: interruptOnReceiveFailed

FIND (around line 992-997):
---------------------------
void DW1000Class::interruptOnReceiveFailed(boolean val) {
	setBit(_sysmask, LEN_SYS_STATUS, LDEERR_BIT, val);
	setBit(_sysmask, LEN_SYS_STATUS, RXFCE_BIT, val);
	setBit(_sysmask, LEN_SYS_STATUS, RXPHE_BIT, val);
	setBit(_sysmask, LEN_SYS_STATUS, RXRFSL_BIT, val);
}

CHANGE TO:
----------
void DW1000Class::interruptOnReceiveFailed(boolean val) {
	setBit(_sysmask, LEN_SYS_MASK, LDEERR_BIT, val);
	setBit(_sysmask, LEN_SYS_MASK, RXFCE_BIT, val);
	setBit(_sysmask, LEN_SYS_MASK, RXPHE_BIT, val);
	setBit(_sysmask, LEN_SYS_MASK, RXRFSL_BIT, val);
}

WHAT CHANGED:
-------------
Replace "LEN_SYS_STATUS" with "LEN_SYS_MASK" in all 4 lines.
(Just change one word, 4 times)

STEPS:
------
1. Open lib/DW1000/src/DW1000.cpp in your text editor
2. Search for "interruptOnReceiveFailed"
3. Change all 4 instances of "LEN_SYS_STATUS" to "LEN_SYS_MASK"
4. Save the file
5. Rebuild your project (important!)
6. Re-upload to Arduino

VERIFICATION:
-------------
After fix, BasicSender/BasicReceiver should work continuously.
See BUG_FIX_GUIDE.md for detailed verification steps.
```

---

## Application Methods

### Method 1: Using patch Command (Linux/Mac/Git Bash)

**Prerequisites**:
- `patch` utility installed (usually pre-installed on Linux/Mac)
- Terminal/command line access

**Steps**:

1. **Navigate to library source directory**:
   ```bash
   cd /path/to/your/project/lib/DW1000/src/
   ```

2. **Download or locate patch file**:
   ```bash
   # If in this project:
   PATCH_FILE="../../../docs/findings/interrupt_bug_fix.patch"

   # Or download from repository:
   curl -O https://raw.githubusercontent.com/YOUR_REPO/interrupt_bug_fix.patch
   PATCH_FILE="interrupt_bug_fix.patch"
   ```

3. **Apply patch**:
   ```bash
   patch -p0 < "$PATCH_FILE"
   ```

4. **Verify application**:
   ```bash
   grep -A 4 "interruptOnReceiveFailed" DW1000.cpp | grep LEN_SYS_MASK
   ```
   Should output 4 lines containing `LEN_SYS_MASK`.

5. **Rebuild project**:
   ```bash
   cd ../../../
   pio run -t clean
   pio run
   ```

**Troubleshooting**:

If patch fails with "Hunk FAILED":
```bash
# Try with fuzz factor
patch -p0 --fuzz=3 < "$PATCH_FILE"

# Or try reverse lookup
patch -p0 -R < "$PATCH_FILE"  # Undo (if already applied)
```

---

### Method 2: Using Git (if library is git-managed)

**Prerequisites**:
- Library is in a git repository
- Git installed

**Steps**:

1. **Navigate to library directory**:
   ```bash
   cd /path/to/your/project/lib/DW1000/
   ```

2. **Create fix branch** (optional but recommended):
   ```bash
   git checkout -b fix-interrupt-bug
   ```

3. **Apply git patch**:
   ```bash
   git am < path/to/0001-fix-interrupt-buffer-overrun.patch
   ```

4. **Verify**:
   ```bash
   git log -1  # Should show the fix commit
   git diff HEAD~1 src/DW1000.cpp  # Should show the changes
   ```

5. **Rebuild project**:
   ```bash
   cd ../../
   pio run -t clean
   pio run
   ```

**Alternative - Manual commit**:

```bash
cd lib/DW1000/

# Edit src/DW1000.cpp manually (apply fix)

# Commit
git add src/DW1000.cpp
git commit -m "Fix critical buffer overrun in interruptOnReceiveFailed()

Changed LEN_SYS_STATUS to LEN_SYS_MASK in all 4 setBit() calls.
This fixes interrupt mask corruption that prevented all hardware
interrupts from functioning."

# Tag
git tag fix-interrupt-v1
```

---

### Method 3: Manual Application (Any Platform)

**Prerequisites**:
- Text editor
- Access to library files

**Steps**:

1. **Locate DW1000.cpp**:
   - PlatformIO: `lib/DW1000/src/DW1000.cpp`
   - Arduino IDE (Linux/Mac): `~/Arduino/libraries/DW1000/src/DW1000.cpp`
   - Arduino IDE (Windows): `Documents\Arduino\libraries\DW1000\src\DW1000.cpp`

2. **Open in text editor**:
   - Visual Studio Code: `code DW1000.cpp`
   - Sublime: `subl DW1000.cpp`
   - Notepad++: `notepad++ DW1000.cpp`
   - Arduino IDE: Open library example, navigate to library

3. **Find the function**:
   - Press Ctrl+F (or Cmd+F on Mac)
   - Search for: `interruptOnReceiveFailed`
   - Should find one function around line 992

4. **Apply fix**:
   - Change line 993: `LEN_SYS_STATUS` → `LEN_SYS_MASK`
   - Change line 994: `LEN_SYS_STATUS` → `LEN_SYS_MASK`
   - Change line 995: `LEN_SYS_STATUS` → `LEN_SYS_MASK`
   - Change line 996: `LEN_SYS_STATUS` → `LEN_SYS_MASK`

5. **Save file**:
   - Press Ctrl+S (or Cmd+S on Mac)

6. **Restart Arduino IDE** (if using Arduino IDE):
   - Close and reopen Arduino IDE
   - This ensures library is recompiled

7. **Rebuild and upload**:
   - Arduino IDE: Click "Verify" then "Upload"
   - PlatformIO: `pio run -t upload`

---

### Method 4: Sed Script (Automated)

**For advanced users - UNIX/Linux/Mac only**:

```bash
#!/bin/bash
# apply_fix.sh - Automated fix script

LIBRARY_PATH="lib/DW1000/src/DW1000.cpp"

if [ ! -f "$LIBRARY_PATH" ]; then
    echo "Error: DW1000.cpp not found at $LIBRARY_PATH"
    exit 1
fi

# Backup original
cp "$LIBRARY_PATH" "$LIBRARY_PATH.backup"

# Apply fix using sed
sed -i.bak 's/setBit(_sysmask, LEN_SYS_STATUS, LDEERR_BIT, val)/setBit(_sysmask, LEN_SYS_MASK, LDEERR_BIT, val)/g' "$LIBRARY_PATH"
sed -i.bak 's/setBit(_sysmask, LEN_SYS_STATUS, RXFCE_BIT, val)/setBit(_sysmask, LEN_SYS_MASK, RXFCE_BIT, val)/g' "$LIBRARY_PATH"
sed -i.bak 's/setBit(_sysmask, LEN_SYS_STATUS, RXPHE_BIT, val)/setBit(_sysmask, LEN_SYS_MASK, RXPHE_BIT, val)/g' "$LIBRARY_PATH"
sed -i.bak 's/setBit(_sysmask, LEN_SYS_STATUS, RXRFSL_BIT, val)/setBit(_sysmask, LEN_SYS_MASK, RXRFSL_BIT, val)/g' "$LIBRARY_PATH"

# Verify
if grep -q "setBit(_sysmask, LEN_SYS_MASK, LDEERR_BIT, val)" "$LIBRARY_PATH" && \
   grep -q "setBit(_sysmask, LEN_SYS_MASK, RXFCE_BIT, val)" "$LIBRARY_PATH" && \
   grep -q "setBit(_sysmask, LEN_SYS_MASK, RXPHE_BIT, val)" "$LIBRARY_PATH" && \
   grep -q "setBit(_sysmask, LEN_SYS_MASK, RXRFSL_BIT, val)" "$LIBRARY_PATH"; then
    echo "✓ Fix applied successfully!"
    echo "Backup saved to: $LIBRARY_PATH.backup"
    exit 0
else
    echo "✗ Fix failed! Restoring backup..."
    mv "$LIBRARY_PATH.backup" "$LIBRARY_PATH"
    exit 1
fi
```

**Usage**:
```bash
chmod +x apply_fix.sh
./apply_fix.sh
```

---

## Verification

### Automated Verification Script

```bash
#!/bin/bash
# verify_fix.sh - Verify fix was applied correctly

LIBRARY_PATH="lib/DW1000/src/DW1000.cpp"

if [ ! -f "$LIBRARY_PATH" ]; then
    echo "Error: DW1000.cpp not found"
    exit 1
fi

echo "Checking for fix in $LIBRARY_PATH..."
echo

# Count occurrences in interruptOnReceiveFailed function
FUNCTION_START=$(grep -n "void DW1000Class::interruptOnReceiveFailed" "$LIBRARY_PATH" | cut -d: -f1)
FUNCTION_END=$((FUNCTION_START + 10))

FIXED_COUNT=$(sed -n "${FUNCTION_START},${FUNCTION_END}p" "$LIBRARY_PATH" | grep -c "LEN_SYS_MASK")
BUGGY_COUNT=$(sed -n "${FUNCTION_START},${FUNCTION_END}p" "$LIBRARY_PATH" | grep -c "LEN_SYS_STATUS")

echo "In interruptOnReceiveFailed():"
echo "  LEN_SYS_MASK occurrences: $FIXED_COUNT (should be 4)"
echo "  LEN_SYS_STATUS occurrences: $BUGGY_COUNT (should be 0)"
echo

if [ "$FIXED_COUNT" -eq 4 ] && [ "$BUGGY_COUNT" -eq 0 ]; then
    echo "✓ FIX VERIFIED - Library is patched correctly!"
    exit 0
else
    echo "✗ FIX NOT APPLIED - Library still has bug!"
    echo
    echo "Current function code:"
    sed -n "${FUNCTION_START},${FUNCTION_END}p" "$LIBRARY_PATH"
    exit 1
fi
```

**Usage**:
```bash
chmod +x verify_fix.sh
./verify_fix.sh
```

### Manual Verification

```bash
# Quick check
grep -A 4 "void DW1000Class::interruptOnReceiveFailed" lib/DW1000/src/DW1000.cpp

# Should output:
# void DW1000Class::interruptOnReceiveFailed(boolean val) {
#     setBit(_sysmask, LEN_SYS_MASK, LDEERR_BIT, val);
#     setBit(_sysmask, LEN_SYS_MASK, RXFCE_BIT, val);
#     setBit(_sysmask, LEN_SYS_MASK, RXPHE_BIT, val);
#     setBit(_sysmask, LEN_SYS_MASK, RXRFSL_BIT, val);
```

### Functional Verification

After applying fix, test with BasicSender/BasicReceiver:

```bash
# Upload and test
cd tests/test_03_04_tx_rx/
pio run -t upload --upload-port /dev/ttyACM0  # Sender
pio run -t upload --upload-port /dev/ttyACM1  # Receiver

# Monitor both
pio device monitor --port /dev/ttyACM0 --baud 9600 &
pio device monitor --port /dev/ttyACM1 --baud 9600 &

# Should see:
# Sender: Continuously transmitting packets #0, #1, #2...
# Receiver: Continuously receiving packets
```

---

## Distribution Instructions

### For Project Teams

**1. Include patch file in repository**:
```
your_project/
├── docs/
│   └── fixes/
│       ├── interrupt_bug_fix.patch
│       ├── BUG_FIX_GUIDE.md
│       └── APPLY_FIX.sh
└── lib/
    └── DW1000/  (apply fix here)
```

**2. Add to README.md**:
```markdown
## CRITICAL: DW1000 Library Fix Required

The arduino-dw1000 library has a critical bug that must be fixed before use.

**To apply fix**:
```bash
cd lib/DW1000/src/
patch -p0 < ../../../docs/fixes/interrupt_bug_fix.patch
```

See [docs/fixes/BUG_FIX_GUIDE.md](docs/fixes/BUG_FIX_GUIDE.md) for details.
```

**3. Add to setup/installation documentation**:
- Include fix application as required step
- Link to BUG_FIX_GUIDE.md
- Verify fix before proceeding with development

**4. CI/CD integration**:
```yaml
# .github/workflows/build.yml
- name: Apply DW1000 library fix
  run: |
    cd lib/DW1000/src/
    patch -p0 < ../../../docs/fixes/interrupt_bug_fix.patch
    cd ../../../

- name: Verify fix applied
  run: |
    if ! grep -q "LEN_SYS_MASK, LDEERR_BIT" lib/DW1000/src/DW1000.cpp; then
      echo "Fix not applied correctly!"
      exit 1
    fi
```

---

### For Public Distribution

**1. Create GitHub Gist**:
- Upload patch file
- Include instructions
- Add link to full documentation
- Example: https://gist.github.com/YOUR_USERNAME/dw1000-interrupt-fix

**2. Create blog post or article**:
```markdown
# Critical Fix for arduino-dw1000 Interrupt Bug

The popular arduino-dw1000 library has a critical bug...

[Detailed explanation]

Download fix: [interrupt_bug_fix.patch](link)

Apply: `patch -p0 < interrupt_bug_fix.patch`

Full guide: [BUG_FIX_GUIDE.md](link)
```

**3. Post to Arduino/DW1000 forums**:
- Arduino Forum
- DW1000/Qorvo forums
- Reddit r/arduino
- Include patch and instructions

**4. Create video tutorial**:
- Demonstrate symptoms
- Show how to apply fix
- Verify fix works
- Upload to YouTube with links in description

---

## Submitting to Maintainers

### GitHub Pull Request Process

**1. Fork the repository**:
- Go to: https://github.com/thotro/arduino-dw1000
- Click "Fork"

**2. Clone your fork**:
```bash
git clone https://github.com/YOUR_USERNAME/arduino-dw1000.git
cd arduino-dw1000
```

**3. Create fix branch**:
```bash
git checkout -b fix-interrupt-buffer-overrun
```

**4. Apply fix**:
```bash
# Edit src/DW1000.cpp manually
# Make the 4-line change
```

**5. Commit with detailed message**:
```bash
git add src/DW1000.cpp
git commit -m "Fix critical buffer overrun in interruptOnReceiveFailed()

The function was using LEN_SYS_STATUS (5 bytes) instead of LEN_SYS_MASK
(4 bytes) when manipulating the _sysmask buffer. This caused a buffer
overrun that corrupted the interrupt mask register, preventing all
hardware interrupts from functioning.

Impact:
- Severity: CRITICAL
- Affects: All code using setDefaults()
- Examples: BasicSender, BasicReceiver, DW1000Ranging
- Result: Complete failure of interrupt-based operations

Fix:
Changed LEN_SYS_STATUS to LEN_SYS_MASK in all 4 setBit() calls in
interruptOnReceiveFailed().

Testing:
- Platform: Arduino Uno with DW1000 modules
- Before: Interrupts never fired, examples hung
- After: All examples work correctly
- Verified: BasicSender/Receiver, DW1000Ranging TAG/ANCHOR

All other interrupt functions (interruptOnReceived, interruptOnSent,
etc.) already use LEN_SYS_MASK correctly. This fix brings
interruptOnReceiveFailed() in line with the rest of the codebase.

References:
- SYS_MASK register: 4 bytes (DW1000Constants.h:104)
- SYS_STATUS register: 5 bytes (DW1000Constants.h:81)
- Both share bit definitions but have different lengths
"
```

**6. Push to your fork**:
```bash
git push origin fix-interrupt-buffer-overrun
```

**7. Create Pull Request**:
- Go to your fork on GitHub
- Click "Pull Request"
- Base: thotro/arduino-dw1000:master
- Head: YOUR_USERNAME/arduino-dw1000:fix-interrupt-buffer-overrun
- Title: `Fix critical buffer overrun in interruptOnReceiveFailed()`
- Description: (use detailed explanation from commit message)
- Click "Create Pull Request"

**8. Follow up**:
- Respond to maintainer comments
- Make requested changes if needed
- Be patient (repository may be unmaintained)

---

### GitHub Issue (Alternative)

If you don't want to create a PR, at least file an issue:

**Go to**: https://github.com/thotro/arduino-dw1000/issues

**Title**: `Critical buffer overrun in interruptOnReceiveFailed() breaks all interrupts`

**Body**:
```markdown
## Summary
Critical buffer overrun bug in `interruptOnReceiveFailed()` prevents all hardware interrupts from working.

## Details
See: [Link to your BUG_FIX_GUIDE.md]

## Patch
Patch file attached: [interrupt_bug_fix.patch]

Or apply manually - change `LEN_SYS_STATUS` to `LEN_SYS_MASK` in 4 places in src/DW1000.cpp line 993-996.

## Impact
- CRITICAL severity
- Affects ALL interrupt-based operations
- Breaks BasicSender/BasicReceiver/DW1000Ranging

## Testing
Verified fix on Arduino Uno + DW1000. All examples now work.
```

---

## Alternative: Using Fixed Fork

If official library remains unmaintained, use a fixed fork.

### Option 1: Use This Project's Fixed Library

**PlatformIO** (`platformio.ini`):
```ini
[env]
lib_deps =
    https://github.com/YOUR_USERNAME/SwarmLoc.git#main
    ; Will use the fixed library in this repo
```

### Option 2: Create Your Own Fork

```bash
# Fork original library
# Clone your fork
git clone https://github.com/YOUR_USERNAME/arduino-dw1000-fixed.git
cd arduino-dw1000-fixed

# Apply fix
# Edit src/DW1000.cpp

# Commit
git add src/DW1000.cpp
git commit -m "Fix critical interrupt buffer overrun bug"
git push

# Tag release
git tag v0.9.1-fixed
git push --tags

# Update README.md
echo "## Fork Info" >> README.md
echo "This fork includes critical bug fix for interrupt handling." >> README.md
echo "See BUGFIX.md for details." >> README.md
git add README.md
git commit -m "Document fork purpose"
git push
```

**Use in projects**:
```ini
# platformio.ini
[env]
lib_deps =
    https://github.com/YOUR_USERNAME/arduino-dw1000-fixed.git#v0.9.1-fixed
```

### Option 3: Use Existing Maintained Forks

Check if these forks have the fix:
- **F-Army/arduino-dw1000-ng**: https://github.com/F-Army/arduino-dw1000-ng
- **leosayous21/DW1000**: https://github.com/leosayous21/DW1000

**Verify they have fix**:
```bash
git clone https://github.com/F-Army/arduino-dw1000-ng.git
cd arduino-dw1000-ng
grep -A 4 "interruptOnReceiveFailed" src/DW1000.cpp | grep LEN_SYS_MASK
```

If they have fix, use them:
```ini
[env]
lib_deps =
    https://github.com/F-Army/arduino-dw1000-ng.git
```

---

## Summary

| Method | Difficulty | Platform | Best For |
|--------|-----------|----------|----------|
| Patch command | Easy | Linux/Mac/Git Bash | Automated deployment |
| Git apply | Easy | Any (with git) | Version controlled projects |
| Manual edit | Easy | Any | All users |
| Sed script | Medium | Linux/Mac | Advanced automation |
| Fork | Medium | Any | Long-term maintenance |

**Recommendation**:
- **Quick fix**: Manual edit (2 minutes)
- **Team project**: Include patch in repo
- **Open source**: Submit PR to original library
- **Long-term**: Use or create maintained fork

---

## Resources

- **Full Guide**: [BUG_FIX_GUIDE.md](BUG_FIX_GUIDE.md)
- **Quick Fix**: [QUICK_FIX.md](QUICK_FIX.md)
- **Original Library**: https://github.com/thotro/arduino-dw1000
- **This Project**: [Link to your project]

---

**Document Version**: 1.0
**Date**: 2026-01-11
**Status**: Patch verified working
