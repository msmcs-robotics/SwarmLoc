# Test 7: Dual-Role Firmware - Complete Index

**Project**: SwarmLoc - GPS-Denied Positioning System
**Created**: 2026-01-11
**Status**: Design Complete - Ready for Testing

---

## Quick Navigation

**Getting Started?** → Read [QUICKSTART.md](QUICKSTART.md) (5 minutes)

**Want to understand the design?** → Read [README.md](README.md) (20 minutes)

**Need technical details?** → Read [DESIGN.md](DESIGN.md) (40 minutes)

**Want overview?** → Read [SUMMARY.md](SUMMARY.md) (10 minutes)

**Ready to test?** → Run `./test_dual_role.sh 1` (automated tests)

---

## File Guide

### 1. QUICKSTART.md (230 lines, 5-minute setup)
**Who**: New users, first-time setup
**What**: Step-by-step guide to get firmware running in 5 minutes

**Contents**:
- Prerequisites checklist
- Node configuration (addresses, IDs)
- Upload instructions (Arduino IDE / PlatformIO)
- Serial connection setup
- Boot verification
- Role switching test
- Common commands reference
- Two-node ranging test
- Troubleshooting quick fixes

**When to read**: Before first upload

---

### 2. README.md (560 lines, comprehensive guide)
**Who**: All users, primary documentation
**What**: Complete user manual and reference

**Contents**:
- Overview and key features
- Architecture explanation (why reset-based?)
- Detailed usage guide
- TDMA configuration and timing
- Complete testing strategy (5 scenarios)
- Design decisions and rationale
- Known limitations and workarounds
- Troubleshooting guide
- Migration path to ESP32
- References

**When to read**: After quick start, for deep understanding

**Key Sections**:
- Section: Architecture → Understand reset-based switching
- Section: TDMA Time Slot Management → Multi-node operation
- Section: Testing Strategy → Systematic validation
- Section: Known Limitations → What to expect

---

### 3. DESIGN.md (740 lines, technical deep-dive)
**Who**: Developers, researchers, maintainers
**What**: Technical design document with all decisions explained

**Contents**:
- Problem statement
- Research findings (library analysis, memory constraints)
- Design constraints (hardware, software, performance)
- Architecture overview (diagrams, state machine)
- Implementation details (EEPROM, reset, TDMA, commands)
- Decision matrix (5 major decisions with rationale)
- Performance analysis (memory, timing, TDMA)
- Testing strategy
- Future work roadmap

**When to read**: For modifications, research, or understanding "why"

**Key Sections**:
- Section 2: Research Findings → Library limitations
- Section 4: Architecture Overview → System design
- Section 6: Design Decisions → Rationale for each choice
- Section 7: Performance Analysis → Memory/timing/TDMA

---

### 4. SUMMARY.md (440 lines, implementation overview)
**Who**: Project managers, stakeholders, reviewers
**What**: High-level summary of what was built and why

**Contents**:
- What was created (overview)
- Files description
- Key design decisions
- Technical specifications
- Research foundation
- Testing strategy overview
- Known limitations
- Next steps (short/medium/long-term)
- Success criteria
- Code statistics
- Quality metrics
- Innovation highlights

**When to read**: For project review or status update

---

### 5. test_07_dual_role.ino (580 lines, main firmware)
**Who**: Developers, users uploading firmware
**What**: Complete Arduino firmware with dual-role support

**Structure**:
```cpp
// Configuration (edit before upload)
const uint8_t NODE_ID = 1;           // Unique per node
const char* MY_ANCHOR_ADDRESS = ...  // Unique per node
const char* MY_TAG_ADDRESS = ...     // Unique per node
#define NUM_NODES 4                   // Total TAG nodes

// Main code sections
setup()           // Initialization, read EEPROM, start role
loop()            // TDMA management, ranging, commands
callbacks         // DW1000 ranging events
commands          // Serial command processing
EEPROM functions  // Role storage
utilities         // Status, reset, memory
```

**What to customize**:
- NODE_ID (line 59)
- NODE_NAME (line 60)
- MY_ANCHOR_ADDRESS (line 64)
- MY_TAG_ADDRESS (line 65)
- NUM_NODES (line 68)

**When to read**: Before uploading, for customization

---

### 6. test_dual_role.sh (380 lines, test automation)
**Who**: Testers, QA engineers
**What**: Automated test suite for validation

**Tests**:
1. Single node boot and status (5 min)
2. Role switching A→T→A (10 min)
3. EEPROM persistence across power cycles (10 min)
4. Anchor-Tag pair ranging (30 min)
5. TDMA multi-tag operation (30 min)

**Usage**:
```bash
./test_dual_role.sh all     # Run all tests
./test_dual_role.sh 1       # Run test 1 only
./test_dual_role.sh 4       # Run ranging test
```

**When to run**: After firmware upload, for validation

---

### 7. platformio.ini (28 lines, build config)
**Who**: PlatformIO users
**What**: Build system configuration

**Contents**:
- Arduino Uno platform
- Library dependencies (SPI, DW1000)
- Include paths
- Serial port settings (115200 baud)
- Build flags

**When to use**: If using PlatformIO instead of Arduino IDE

---

### 8. ARCHITECTURE_DIAGRAM.txt (visual diagrams)
**Who**: Visual learners, system designers
**What**: ASCII art diagrams of architecture

**Diagrams**:
- System overview
- Firmware architecture
- State machine
- TDMA timing
- Memory layout
- EEPROM layout
- Ranging protocol
- Serial command flow
- Typical deployment
- Configuration example

**When to read**: To visualize system design

---

### 9. FILE_STRUCTURE.txt (directory tree)
**Who**: Anyone navigating the project
**What**: Complete file tree with descriptions

**Contents**:
- Directory structure
- File sizes and line counts
- Purpose of each file
- Key features summary
- Usage instructions
- Statistics

**When to read**: First time exploring project

---

### 10. INDEX.md (this file)
**Who**: Everyone
**What**: Master navigation and file guide

---

## Reading Paths

### Path 1: First-Time User (20 minutes)
1. **INDEX.md** (this file) - 2 minutes
2. **QUICKSTART.md** - 5 minutes (setup)
3. **README.md** (Sections: Overview, Usage, Commands) - 10 minutes
4. Upload firmware and test - 3 minutes

**Goal**: Get firmware running

---

### Path 2: Developer/Researcher (90 minutes)
1. **SUMMARY.md** - 10 minutes (overview)
2. **README.md** - 20 minutes (full read)
3. **DESIGN.md** - 40 minutes (technical depth)
4. **test_07_dual_role.ino** - 20 minutes (code review)

**Goal**: Understand design and implementation

---

### Path 3: Tester/QA (45 minutes)
1. **QUICKSTART.md** - 5 minutes (setup)
2. **README.md** (Section: Testing Strategy) - 10 minutes
3. **test_dual_role.sh** - 5 minutes (review tests)
4. Run automated tests - 25 minutes (execution)

**Goal**: Validate firmware functionality

---

### Path 4: Project Manager (15 minutes)
1. **SUMMARY.md** - 10 minutes (complete overview)
2. **README.md** (Sections: Overview, Limitations) - 5 minutes

**Goal**: Understand status and next steps

---

## Document Statistics

| File | Size | Lines | Type | Audience |
|------|------|-------|------|----------|
| test_07_dual_role.ino | 16 KB | 580 | Code | Developers |
| README.md | 21 KB | 560 | Docs | All Users |
| DESIGN.md | 30 KB | 740 | Docs | Developers |
| QUICKSTART.md | 6 KB | 230 | Docs | New Users |
| SUMMARY.md | 15 KB | 440 | Docs | Managers |
| test_dual_role.sh | 13 KB | 380 | Script | Testers |
| platformio.ini | 530 B | 28 | Config | PlatformIO |
| ARCHITECTURE_DIAGRAM.txt | 10 KB | 330 | Docs | Visual |
| FILE_STRUCTURE.txt | 4 KB | 130 | Docs | Navigation |
| INDEX.md | 5 KB | 200 | Docs | Navigation |

**Total**: ~120 KB, ~3,600 lines, 10 files

**Documentation-to-Code Ratio**: 4:1 (exceptional coverage)

---

## Key Concepts Quick Reference

### Reset-Based Role Switching
Instead of runtime role switching (not possible with DW1000Ranging library), firmware:
1. Receives serial command ('A' or 'T')
2. Saves role to EEPROM
3. Performs software reset (jmp 0)
4. Reboots with new role from EEPROM

**Trade-off**: 5-second downtime vs. weeks of library rewrite

### TDMA (Time Division Multiple Access)
Prevents collisions when multiple TAG nodes range with same ANCHOR:
- Each TAG has assigned time slot (150ms default)
- Only transmits during its slot
- Other nodes stay idle during others' slots

**Example**: 3 tags → 450ms frame → 2.2 Hz per tag

### Single-Role Architecture
Only one role active at a time (ANCHOR or TAG, not both):
- Saves memory (~800 bytes SRAM)
- Works within Arduino Uno 2KB limit
- Sufficient for reset-based switching

### EEPROM Persistence
Role stored in non-volatile memory:
- Survives power cycles
- Only 2 bytes used (role + magic byte)
- 100,000 write cycle lifetime

---

## Common Tasks

### Task: Upload Firmware First Time
1. Read [QUICKSTART.md](QUICKSTART.md)
2. Edit node configuration in `test_07_dual_role.ino`
3. Upload via Arduino IDE
4. Connect serial at 115200 baud
5. Verify boot message

### Task: Change Role
1. Connect serial terminal (115200 baud)
2. Type `A` (for ANCHOR) or `T` (for TAG)
3. Wait 5 seconds for reset
4. Verify new role in boot message

### Task: Test Ranging
1. Configure Node 1: Type `A` → becomes ANCHOR
2. Configure Node 2: Type `T` → becomes TAG
3. Place nodes 1-2 meters apart
4. Watch for [RANGE] messages
5. Verify distance readings

### Task: Configure TDMA Multi-Tag
1. Edit firmware: Set `NUM_NODES` to total TAGs (2, 3, or 4)
2. Set each TAG's `NODE_ID` uniquely (1, 2, 3, 4)
3. Upload to each TAG node
4. Configure one node as ANCHOR
5. All TAGs will range in turn (no collisions)

### Task: Check Status
1. Connect serial terminal
2. Type `S`
3. Review output:
   - Current role
   - Range count
   - Free RAM (~450 bytes expected)
   - TDMA configuration

### Task: Troubleshoot No Ranging
1. Apply interrupt bug fix (see README Known Limitations #5)
2. Verify one node is ANCHOR, other is TAG
3. Check addresses are unique (different last byte)
4. Ensure nodes 0.5-10m apart
5. Verify IRQ pin (D2) connected

---

## Success Checklist

**Phase 1: Compilation (5 minutes)**
- [ ] Firmware compiles without errors
- [ ] Library paths correct
- [ ] No missing dependencies

**Phase 2: Basic Operation (10 minutes)**
- [ ] Serial output appears at 115200 baud
- [ ] Boot message shows node name and role
- [ ] Command 'S' displays status
- [ ] Free RAM ~450 bytes

**Phase 3: Role Switching (10 minutes)**
- [ ] Command 'A' switches to ANCHOR (after reset)
- [ ] Command 'T' switches to TAG (after reset)
- [ ] Role persists after power cycle
- [ ] Reset time ~5 seconds

**Phase 4: Ranging (30 minutes)**
- [ ] Two nodes range successfully (1 ANCHOR + 1 TAG)
- [ ] [RANGE] messages appear
- [ ] Distance values reasonable (0.5-10m)
- [ ] Update rate ≥1 Hz

**Phase 5: TDMA (optional, 30 minutes)**
- [ ] Multiple TAGs configured with unique NODE_IDs
- [ ] No message collisions observed
- [ ] All TAGs range with ANCHOR
- [ ] Update rate per TAG: ~1.7-3.3 Hz

---

## Troubleshooting Index

| Problem | Solution | Reference |
|---------|----------|-----------|
| Won't compile | Check library paths | QUICKSTART.md |
| No serial output | Verify baud rate (115200) | QUICKSTART.md |
| Role won't switch | Check EEPROM init | README.md Section |
| No ranging | Apply interrupt fix | README.md Known Limitations |
| TDMA collisions | Verify NODE_ID unique | README.md TDMA Section |
| Low free RAM | Reduce NUM_NODES | DESIGN.md Memory |
| Slow update rate | Check NUM_NODES config | DESIGN.md Performance |

---

## External References

### Research Documents
- **DUAL_ROLE_ARCHITECTURE.md** (48KB, primary reference)
  - Path: `../../docs/findings/DUAL_ROLE_ARCHITECTURE.md`
  - Comprehensive research on dual-role implementation

### Related Tests
- **test_06_ranging**: Baseline anchor/tag examples
  - Path: `../test_06_ranging/`
  - Interrupt bug fix instructions

### Library
- **arduino-dw1000 v0.9**: DW1000Ranging library
  - Path: `../../lib/DW1000/`
  - Source code and examples

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-01-11 | Initial release |
|     |            | - Complete firmware implementation |
|     |            | - Comprehensive documentation |
|     |            | - Automated test suite |
|     |            | - Ready for hardware testing |

---

## Contact and Support

**Project**: SwarmLoc
**Hardware**: Arduino Uno + DW1000 (PCL298336 v1.3)
**Firmware**: test_07_dual_role v1.0
**Status**: Design Complete - Ready for Testing

**Directory**: `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_07_dual_role/`

---

## Next Actions

### Immediate (Today)
1. Review all documentation (this index)
2. Read QUICKSTART.md
3. Verify firmware compiles

### Short-Term (This Week)
1. Upload firmware to test hardware
2. Test basic operation (boot, commands)
3. Test role switching
4. Document any issues

### Medium-Term (Next 2 Weeks)
1. Test ranging with 2 nodes
2. Measure accuracy
3. Test TDMA with 3-4 nodes
4. Collect performance data

### Long-Term (Future)
1. Apply interrupt fix if needed
2. Port to ESP32
3. Field test on drones
4. Production deployment

---

**This index is your starting point. Choose your reading path above and dive in!**

**For quick setup**: Start with [QUICKSTART.md](QUICKSTART.md)

**For deep understanding**: Start with [README.md](README.md)

**For technical details**: Start with [DESIGN.md](DESIGN.md)

---

**End of Index**

**Created**: 2026-01-11
**Status**: Complete
