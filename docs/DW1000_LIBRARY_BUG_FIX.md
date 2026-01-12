# DW1000 Library Critical Bug Fix - Overview

**IMPORTANT**: This document provides a quick overview and links to comprehensive documentation about a critical bug in the arduino-dw1000 library.

---

## What's the Issue?

The arduino-dw1000 library (v0.9 and earlier) contains a **critical buffer overrun bug** that prevents ALL interrupt-based operations from working.

### Impact

- **Severity**: CRITICAL
- **Affects**: All code using `setDefaults()` - which includes virtually all examples
- **Result**: Complete failure of BasicSender, BasicReceiver, DW1000Ranging, and all interrupt-based communication

### Symptoms

- BasicSender transmits one packet then hangs
- BasicReceiver initializes but never receives packets
- DW1000Ranging devices never discover each other
- Interrupt callbacks never execute
- No error messages - just silent failure

---

## The Fix (2 Minutes)

Change 4 constants in one function:

**File**: `DWS1000_UWB/lib/DW1000/src/DW1000.cpp` (lines 992-997)

**Change**: Replace `LEN_SYS_STATUS` with `LEN_SYS_MASK` in 4 places

### Quick Apply (Linux/Mac)

```bash
cd DWS1000_UWB/lib/DW1000/src/
patch -p0 < ../../../docs/findings/interrupt_bug_fix.patch
```

### Manual Fix (Any Platform)

Edit `DW1000.cpp` and find `interruptOnReceiveFailed` function:

**Change from**:
```cpp
void DW1000Class::interruptOnReceiveFailed(boolean val) {
    setBit(_sysmask, LEN_SYS_STATUS, LDEERR_BIT, val);
    setBit(_sysmask, LEN_SYS_STATUS, RXFCE_BIT, val);
    setBit(_sysmask, LEN_SYS_STATUS, RXPHE_BIT, val);
    setBit(_sysmask, LEN_SYS_STATUS, RXRFSL_BIT, val);
}
```

**To**:
```cpp
void DW1000Class::interruptOnReceiveFailed(boolean val) {
    setBit(_sysmask, LEN_SYS_MASK, LDEERR_BIT, val);
    setBit(_sysmask, LEN_SYS_MASK, RXFCE_BIT, val);
    setBit(_sysmask, LEN_SYS_MASK, RXPHE_BIT, val);
    setBit(_sysmask, LEN_SYS_MASK, RXRFSL_BIT, val);
}
```

---

## Documentation

### Comprehensive Guides

1. **[BUG_FIX_GUIDE.md](findings/BUG_FIX_GUIDE.md)** (30+ pages)
   - Complete technical explanation
   - Why the bug breaks everything
   - Step-by-step fix instructions
   - Before/after code comparison
   - Multiple verification methods
   - FAQ and troubleshooting

2. **[LIBRARY_PATCH.md](findings/LIBRARY_PATCH.md)** (20+ pages)
   - Ready-to-use patch files
   - Multiple application methods
   - Distribution instructions
   - How to submit to maintainers
   - Creating fixed forks

3. **[QUICK_FIX.md](findings/QUICK_FIX.md)** (2 pages)
   - 5-minute quick fix guide
   - Essential information only

### Supporting Documentation

4. **[interrupt_debugging.md](findings/interrupt_debugging.md)**
   - Complete technical analysis
   - Debugging process documentation
   - Alternative workarounds

5. **[INTERRUPT_ISSUE_SUMMARY.md](findings/INTERRUPT_ISSUE_SUMMARY.md)**
   - Executive summary
   - Test results
   - Impact analysis

6. **Patch File**: [interrupt_bug_fix.patch](findings/interrupt_bug_fix.patch)
   - Ready to apply
   - Standard unified diff format

---

## Status in This Project

**✅ FIX APPLIED**: The bug has been fixed in this project's library (2026-01-11)

All tests now work correctly:
- BasicSender/BasicReceiver: Working
- DW1000Ranging TAG/ANCHOR: Working
- All interrupts: Firing correctly

---

## For New Users

If you're using the **original unmodified arduino-dw1000 library** from GitHub:

1. **You MUST apply this fix before testing**
2. **Takes only 2 minutes**
3. **Without the fix, nothing will work**

Follow the instructions in [BUG_FIX_GUIDE.md](findings/BUG_FIX_GUIDE.md).

---

## Library Maintainer Status

**Research Findings** (2026-01-11):

### Original Library
- **Repository**: https://github.com/thotro/arduino-dw1000
- **Last Commit**: May 11, 2020 (v0.9)
- **Status**: Appears unmaintained (5+ years without updates)
- **Open Issues**: Repository has issues/PRs but no recent activity

### This Bug
- **Not reported** in official repository issues (as of search date)
- **May exist in forks** - needs verification
- **Community likely applied local fixes** without reporting back

### Alternative Libraries

Potentially maintained forks (need verification of fix status):

1. **F-Army/arduino-dw1000-ng**
   - https://github.com/F-Army/arduino-dw1000-ng
   - Status: Unknown if this fork has the fix

2. **leosayous21/DW1000**
   - https://github.com/leosayous21/DW1000
   - Ranging-focused fork
   - Status: Unknown if this fork has the fix

### Recommendation

**For immediate use**:
- Apply the fix to your local copy (2 minutes)
- Continue using thotro/arduino-dw1000 with fix

**For production**:
- Consider creating your own maintained fork
- Or verify alternative libraries have the fix
- Document the requirement for your team

**For the community**:
- Submit issue to original repository (document exists)
- Submit pull request with fix
- Share fix in Arduino forums/communities
- See [LIBRARY_PATCH.md](findings/LIBRARY_PATCH.md) for submission instructions

---

## Technical Summary

### The Bug

**Location**: `DW1000.cpp::interruptOnReceiveFailed()`

**Problem**: Uses `LEN_SYS_STATUS` (5 bytes) instead of `LEN_SYS_MASK` (4 bytes) when manipulating 4-byte `_sysmask` buffer.

**Result**:
1. Buffer overrun writes beyond buffer boundary
2. Corrupts adjacent memory (likely `_chanctrl` array)
3. Corrupts DW1000 interrupt mask register
4. Hardware never generates interrupts
5. All interrupt-based operations fail

**Why Critical**:
- Affects `setDefaults()` which is called by virtually all examples
- Silent failure - no error messages
- Completely breaks interrupt functionality
- Platform-independent (affects all hardware)

### The Fix

**Change**: 4 constants in 4 lines
**Impact**: Restores correct interrupt mask configuration
**Risk**: None - corrects buffer size to match actual buffer
**Testing**: Verified on Arduino Uno + DW1000 modules

All other interrupt functions already use the correct constant (`LEN_SYS_MASK`). This fix brings `interruptOnReceiveFailed()` in line with the rest of the codebase.

---

## Verification

After applying fix, verify with BasicSender/BasicReceiver:

**Expected Results**:
- Sender continuously transmits (incrementing packet numbers)
- Receiver continuously receives packets
- No hanging or stopping
- Callbacks execute properly

See [BUG_FIX_GUIDE.md](findings/BUG_FIX_GUIDE.md) for complete verification procedures.

---

## Help and Support

### Documentation Files

Located in `DWS1000_UWB/docs/findings/`:
- `BUG_FIX_GUIDE.md` - Complete guide (start here)
- `LIBRARY_PATCH.md` - Patch distribution guide
- `QUICK_FIX.md` - Quick reference
- `interrupt_bug_fix.patch` - Patch file
- `interrupt_debugging.md` - Technical analysis
- `INTERRUPT_ISSUE_SUMMARY.md` - Executive summary

### Project Files

- **Library Setup**: `DWS1000_UWB/docs/findings/DW1000_LIBRARY_SETUP.md`
- **Test Documentation**: `DWS1000_UWB/tests/README.md`
- **Test Results**: `DWS1000_UWB/docs/findings/TEST_RESULTS.md`

### Getting Help

If you have questions or issues:

1. Read [BUG_FIX_GUIDE.md](findings/BUG_FIX_GUIDE.md) - answers most questions
2. Check test documentation for verification examples
3. Review technical analysis in interrupt_debugging.md
4. Verify fix was applied correctly
5. Check hardware connections if issues persist

---

## Contributing

### Report to Original Library

Instructions for submitting to original maintainers: [LIBRARY_PATCH.md](findings/LIBRARY_PATCH.md#submitting-to-maintainers)

### Share with Community

- Post to Arduino forums
- Share on Reddit r/arduino
- Create GitHub Gist
- Blog posts welcome

**Please link back to this documentation** so others can benefit from the detailed analysis and instructions.

---

## Timeline

- **Bug Discovery**: 2026-01-11
- **Root Cause Identified**: 2026-01-11
- **Fix Developed**: 2026-01-11
- **Fix Applied to Project**: 2026-01-11
- **Documentation Created**: 2026-01-11
- **Testing Completed**: 2026-01-11

**Status**: Fix verified and working. All tests passing.

---

## Quick Links

| Document | Purpose | Pages | Start Here? |
|----------|---------|-------|-------------|
| [BUG_FIX_GUIDE.md](findings/BUG_FIX_GUIDE.md) | Complete fix guide | 30+ | ✅ YES |
| [LIBRARY_PATCH.md](findings/LIBRARY_PATCH.md) | Patch distribution | 20+ | For sharing |
| [QUICK_FIX.md](findings/QUICK_FIX.md) | Quick reference | 2 | If experienced |
| [interrupt_debugging.md](findings/interrupt_debugging.md) | Technical analysis | 13 | For deep dive |
| [INTERRUPT_ISSUE_SUMMARY.md](findings/INTERRUPT_ISSUE_SUMMARY.md) | Executive summary | 8 | For overview |

---

**Document Version**: 1.0
**Date**: 2026-01-11
**Status**: Active - fix verified working
**Project**: SwarmLoc / DWS1000_UWB

---

**CRITICAL REMINDER**: Apply the fix before any testing. Without it, nothing will work!
