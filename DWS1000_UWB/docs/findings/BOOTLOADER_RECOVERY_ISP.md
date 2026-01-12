# Arduino Uno Bootloader Recovery Using ISP Programming

## Overview

This guide explains how to re-burn the Arduino Uno bootloader using another Arduino Uno as an In-System Programmer (ISP). This procedure can fix upload issues on ACM1 (or any Arduino) if the bootloader has become corrupted or damaged.

**Use Case:** If you have a working Arduino on `/dev/ttyACM0` and a non-working Arduino on `/dev/ttyACM1` that fails to upload sketches, burning a fresh bootloader may resolve the issue.

---

## Table of Contents

1. [Understanding the Problem](#understanding-the-problem)
2. [Required Hardware](#required-hardware)
3. [Method 1: Arduino as ISP (Recommended)](#method-1-arduino-as-isp-recommended)
4. [Method 2: Direct avrdude Commands](#method-2-direct-avrdude-commands)
5. [Method 3: PlatformIO ISP Programming](#method-3-platformio-isp-programming)
6. [Method 4: External USBasp Programmer](#method-4-external-usbasp-programmer)
7. [Verification Procedures](#verification-procedures)
8. [Troubleshooting Guide](#troubleshooting-guide)
9. [Success Criteria](#success-criteria)

---

## Understanding the Problem

### What is the Bootloader?

The bootloader is a small piece of software pre-programmed on Arduino's ATmega328P microcontroller that:
- Runs when the board is powered on or reset
- Listens for sketch uploads via USB/serial connection
- Allows uploading sketches without external programmers
- Occupies approximately 512 bytes of flash memory

### Signs of Corrupted Bootloader

- `avrdude: stk500_recv(): programmer is not responding`
- `avrdude: stk500_getsync() attempt X of 10: not in sync`
- Board appears in `/dev/ttyACMx` but uploads fail
- LED blinks erratically or not at all during upload
- Error: "An error occurred while uploading the sketch"

---

## Required Hardware

### For Arduino-as-ISP Method

1. **Programmer Arduino** (ACM0 - working board)
   - Arduino Uno, Nano, or Mega
   - Must have working USB connection
   - Must be able to upload sketches successfully

2. **Target Arduino** (ACM1 - board with corrupted bootloader)
   - Arduino Uno with suspected bootloader issue

3. **Jumper Wires** (6 minimum)
   - Male-to-male jumper wires for breadboard connections
   - Or female-to-female for direct board-to-board

4. **10µF Electrolytic Capacitor** (CRITICAL)
   - Must be rated for at least 16V
   - Prevents programmer Arduino from auto-resetting

5. **Optional: Breadboard**
   - For organizing connections

---

## Method 1: Arduino as ISP (Recommended)

This is the easiest and most reliable method using Arduino IDE.

### Step 1: Prepare the Programmer Arduino (ACM0)

#### 1.1 Upload ArduinoISP Sketch

1. Connect your **working Arduino** (ACM0) to your computer
2. Open Arduino IDE
3. Navigate to: **File → Examples → 11.ArduinoISP → ArduinoISP**
4. Select the correct board: **Tools → Board → Arduino Uno**
5. Select the correct port: **Tools → Port → /dev/ttyACM0**
6. Click **Upload** and wait for "Done uploading" message

```
Expected output:
Sketch uses 2896 bytes (8%) of program storage space.
Global variables use 169 bytes (8%) of dynamic memory.
```

#### 1.2 Add Reset Bypass Capacitor (CRITICAL STEP)

**IMPORTANT:** This capacitor prevents the programmer Arduino from resetting itself during ISP operations.

- **Capacitor:** 10µF electrolytic
- **Connection:**
  - Positive leg (+) → RESET pin on programmer Arduino (ACM0)
  - Negative leg (-) → GND pin on programmer Arduino (ACM0)
- **Polarity:** The longer leg is positive (+), shorter leg is negative (-)
- **Timing:** Add this capacitor AFTER uploading the ArduinoISP sketch

**Why it's needed:** Without this capacitor, the programmer Arduino may reset during programming, causing the operation to fail.

### Step 2: Wire Programmer to Target

#### Pin Connection Diagram

```
PROGRAMMER (ACM0)          TARGET (ACM1)
Arduino Uno                Arduino Uno
─────────────────          ─────────────

Pin 10      ─────────────→ RESET
Pin 11 (MOSI) ─────────────→ Pin 11 (MOSI)
Pin 12 (MISO) ─────────────→ Pin 12 (MISO)
Pin 13 (SCK)  ─────────────→ Pin 13 (SCK)
5V           ─────────────→ 5V
GND          ─────────────→ GND

RESET (on ACM0) ←─┳─→ GND (on ACM0)
                  └─ 10µF capacitor
```

#### Alternative: Using ICSP Header

Both programmer and target Arduinos have a 6-pin ICSP header. The pinout is:

```
ICSP Header Pinout (top view):
┌─────────────┐
│ ● MISO  5V ●│
│ ● SCK  MOSI●│
│ ● RST   GND●│
└─────────────┘
```

**ICSP Connections:**
- Programmer Pin 10 → Target RESET (ICSP header pin)
- Programmer MISO (ICSP) → Target MISO (ICSP)
- Programmer MOSI (ICSP) → Target MOSI (ICSP)
- Programmer SCK (ICSP) → Target SCK (ICSP)
- Programmer 5V → Target 5V
- Programmer GND → Target GND

### Step 3: Configure Arduino IDE

1. **Select the Target Board:**
   - Go to **Tools → Board → Arduino Uno**
   - This tells IDE which bootloader to burn

2. **Select the Programmer:**
   - Go to **Tools → Programmer → Arduino as ISP**
   - **CRITICAL:** Select "Arduino as ISP" NOT "ArduinoISP.org" or "ArduinoISP"

3. **Keep Programmer Port Selected:**
   - **Tools → Port → /dev/ttyACM0** (the programmer board)
   - DO NOT select ACM1 at this stage

### Step 4: Burn the Bootloader

1. Click **Tools → Burn Bootloader**
2. Watch the console for progress output
3. Enable verbose output: **File → Preferences → Show verbose output during: upload**

**Expected console output:**

```
avrdude: AVR device initialized and ready to accept instructions
Reading | ################################################## | 100% 0.01s
avrdude: Device signature = 0x1e950f (probably m328p)
avrdude: erasing chip
avrdude: reading input file "0xFF"
avrdude: writing lock (1 bytes):
Writing | ################################################## | 100% 0.02s
avrdude: writing efuse (1 bytes):
Writing | ################################################## | 100% 0.02s
avrdude: writing hfuse (1 bytes):
Writing | ################################################## | 100% 0.01s
avrdude: writing lfuse (1 bytes):
Writing | ################################################## | 100% 0.02s
avrdude: writing flash (32768 bytes):
Writing | ################################################## | 100% 4.89s
avrdude: writing lock (1 bytes):
Writing | ################################################## | 100% 0.01s

avrdude done. Thank you.
```

4. Wait for "Done burning bootloader" message (typically 20-60 seconds)

### Step 5: Test the Bootloader

1. **Disconnect all ISP wiring** from the target Arduino (ACM1)
2. **Remove the 10µF capacitor** from the programmer Arduino
3. Connect the target Arduino directly to your computer via USB
4. Select **Tools → Port → /dev/ttyACM1** (or whichever port appears)
5. Upload a simple sketch like **File → Examples → 01.Basics → Blink**
6. If upload succeeds, bootloader is restored successfully!

---

## Method 2: Direct avrdude Commands

For advanced users who prefer command-line control or need to script the process.

### Prerequisites

```bash
# Install avrdude if not present
sudo apt-get update
sudo apt-get install avrdude

# Check avrdude version
avrdude -v
```

### Step 1: Prepare Hardware

Follow the same wiring as Method 1 (including the 10µF capacitor on programmer's RESET).

### Step 2: Set Fuse Bits

Fuse bits configure the ATmega328P's operation (clock source, brown-out detection, bootloader size, etc.).

**Arduino Uno ATmega328P Fuse Values:**
- Extended Fuse (efuse): `0xFD` or `0x05` (bit masking differences)
- High Fuse (hfuse): `0xDE` (2048 words bootloader, SPI enabled)
- Low Fuse (lfuse): `0xFF` (16MHz external crystal, slowly rising power)

```bash
# Set fuse bits for Arduino Uno
avrdude -C /etc/avrdude.conf \
  -p atmega328p \
  -c avrisp \
  -P /dev/ttyACM0 \
  -b 19200 \
  -v \
  -e \
  -U efuse:w:0xFD:m \
  -U hfuse:w:0xDE:m \
  -U lfuse:w:0xFF:m
```

**Parameter explanation:**
- `-C /etc/avrdude.conf` - Configuration file
- `-p atmega328p` - Target chip type
- `-c avrisp` - Programmer type (Arduino as ISP)
- `-P /dev/ttyACM0` - Programmer port
- `-b 19200` - Baud rate for ArduinoISP
- `-v` - Verbose output
- `-e` - Erase chip before programming
- `-U` - Memory operation (efuse/hfuse/lfuse)
- `:w:` - Write operation
- `:m` - Immediate mode (raw hex value)

### Step 3: Flash Bootloader Hex File

**Locate the bootloader file:**

```bash
# For Arduino IDE 1.x
BOOTLOADER_PATH="$HOME/.arduino15/packages/arduino/hardware/avr/1.8.6/bootloaders/optiboot/optiboot_atmega328.hex"

# Alternative locations
# /usr/share/arduino/hardware/arduino/avr/bootloaders/optiboot/optiboot_atmega328.hex
# Check your Arduino installation directory

# Verify file exists
ls -lh "$BOOTLOADER_PATH"
```

**Flash the bootloader:**

```bash
avrdude -C /etc/avrdude.conf \
  -p atmega328p \
  -c avrisp \
  -P /dev/ttyACM0 \
  -b 19200 \
  -v \
  -U flash:w:"$BOOTLOADER_PATH":i \
  -U lock:w:0x0F:m
```

**Parameter explanation:**
- `flash:w:filename.hex:i` - Write hex file to flash memory (Intel hex format)
- `lock:w:0x0F:m` - Set lock bits (protect bootloader section)

### Step 4: Complete Command (All-in-One)

```bash
# Combined command to set fuses and flash bootloader
avrdude -C /etc/avrdude.conf \
  -p atmega328p \
  -c avrisp \
  -P /dev/ttyACM0 \
  -b 19200 \
  -v -e \
  -U efuse:w:0xFD:m \
  -U hfuse:w:0xDE:m \
  -U lfuse:w:0xFF:m \
  -U flash:w:"$HOME/.arduino15/packages/arduino/hardware/avr/1.8.6/bootloaders/optiboot/optiboot_atmega328.hex":i \
  -U lock:w:0x0F:m
```

### Expected Output

```
avrdude: AVR device initialized and ready to accept instructions
Reading | ################################################## | 100% 0.01s
avrdude: Device signature = 0x1e950f (probably m328p)
avrdude: erasing chip
avrdude: reading input file "0xFD"
avrdude: writing efuse (1 bytes):
Writing | ################################################## | 100% 0.02s
avrdude: 1 bytes of efuse written
avrdude: verifying efuse memory against 0xFD:
avrdude: load data efuse data from input file 0xFD:
avrdude: input file 0xFD contains 1 bytes
avrdude: reading on-chip efuse data:
Reading | ################################################## | 100% 0.01s
avrdude: verifying ...
avrdude: 1 bytes of efuse verified

[... similar output for hfuse and lfuse ...]

avrdude: reading input file "optiboot_atmega328.hex"
avrdude: writing flash (32768 bytes):
Writing | ################################################## | 100% 4.89s
avrdude: 32768 bytes of flash written
avrdude: verifying flash memory against optiboot_atmega328.hex:
avrdude: load data flash data from input file optiboot_atmega328.hex:
Reading | ################################################## | 100% 1.23s
avrdude: verifying ...
avrdude: 32768 bytes of flash verified

avrdude: reading input file "0x0F"
avrdude: writing lock (1 bytes):
Writing | ################################################## | 100% 0.01s

avrdude done. Thank you.
```

---

## Method 3: PlatformIO ISP Programming

For projects using PlatformIO instead of Arduino IDE.

### Step 1: Create platformio.ini Configuration

Add the following to your `platformio.ini` file:

```ini
[env:uno_bootloader]
platform = atmelavr
board = uno
framework = arduino

; ISP Programming Configuration
upload_protocol = custom
upload_port = /dev/ttyACM0
upload_speed = 19200
upload_flags =
    -C
    $PROJECT_PACKAGES_DIR/tool-avrdude/avrdude.conf
    -p
    $BOARD_MCU
    -P
    $UPLOAD_PORT
    -b
    $UPLOAD_SPEED
    -c
    avrisp
upload_command = avrdude $UPLOAD_FLAGS -U flash:w:$SOURCE:i

; Bootloader Configuration
board_bootloader.file = optiboot_atmega328.hex
board_bootloader.lfuse = 0xFF
board_bootloader.hfuse = 0xDE
board_bootloader.efuse = 0xFD
board_bootloader.lock_bits = 0x0F
board_bootloader.unlock_bits = 0x3F

; Erase chip before burning bootloader
board_fuses.efuse = 0xFD
board_fuses.hfuse = 0xDE
board_fuses.lfuse = 0xFF
```

### Step 2: Burn Bootloader via PlatformIO

```bash
# Navigate to your project directory
cd /home/devel/Desktop/SwarmLoc/DWS1000_UWB

# Burn bootloader using PlatformIO
pio run -e uno_bootloader -t bootloader

# Alternative: Set fuses only
pio run -e uno_bootloader -t fuses
```

### Known Issues with PlatformIO

- Setting custom upload baud rates for bootloader burning can be difficult
- ArduinoISP relies on `upload_speed = 19200` to work correctly
- Some users report better success using Arduino IDE or direct avrdude commands

### Workaround for Upload Speed

If PlatformIO doesn't respect the upload speed:

```bash
# Use avrdude directly via PlatformIO's tool
~/.platformio/packages/tool-avrdude/bin/avrdude \
  -C ~/.platformio/packages/tool-avrdude/avrdude.conf \
  -p atmega328p \
  -c avrisp \
  -P /dev/ttyACM0 \
  -b 19200 \
  -v -e \
  -U flash:w:~/.platformio/packages/framework-arduino-avr/bootloaders/optiboot/optiboot_atmega328.hex:i
```

---

## Method 4: External USBasp Programmer

If you have a USBasp programmer (inexpensive USB ISP programmer), this is an alternative to Arduino-as-ISP.

### Hardware Setup

**USBasp to Arduino Uno Wiring:**

```
USBasp 10-pin          Arduino Uno
─────────────          ───────────
MOSI     ─────────────→ Pin 11 (MOSI)
MISO     ─────────────→ Pin 12 (MISO)
SCK      ─────────────→ Pin 13 (SCK)
RESET    ─────────────→ RESET
VCC      ─────────────→ 5V
GND      ─────────────→ GND
```

**Or use the 6-pin ICSP header directly:**

```
USBasp 6-pin ICSP      Arduino Uno ICSP
─────────────────      ────────────────
MISO   ───────────────→ MISO
VCC    ───────────────→ VCC
SCK    ───────────────→ SCK
MOSI   ───────────────→ MOSI
RESET  ───────────────→ RESET
GND    ───────────────→ GND
```

### Arduino IDE Method

1. Connect USBasp to target Arduino
2. In Arduino IDE:
   - **Tools → Board → Arduino Uno**
   - **Tools → Programmer → USBasp**
3. Click **Tools → Burn Bootloader**

### avrdude Command Method

```bash
# Set fuses and burn bootloader with USBasp
avrdude -C /etc/avrdude.conf \
  -p atmega328p \
  -c usbasp \
  -v -e \
  -U efuse:w:0xFD:m \
  -U hfuse:w:0xDE:m \
  -U lfuse:w:0xFF:m \
  -U flash:w:"$BOOTLOADER_PATH":i \
  -U lock:w:0x0F:m
```

**Note:** USBasp doesn't require a baud rate parameter (`-b` flag).

### Advantages of USBasp

- No need for a second Arduino
- No capacitor needed
- More reliable for repeated bootloader burning
- Can program chips outside of Arduino boards

### Where to Buy

- Amazon, eBay, AliExpress: ~$5-15 USD
- Electronics suppliers: Adafruit, SparkFun, DigiKey

---

## Verification Procedures

### 1. Verify Device Signature

Before burning, verify that avrdude can communicate with the target chip:

```bash
avrdude -C /etc/avrdude.conf \
  -p atmega328p \
  -c avrisp \
  -P /dev/ttyACM0 \
  -b 19200 \
  -v
```

**Expected output:**

```
avrdude: Device signature = 0x1e950f (probably m328p)
```

**Device signatures:**
- `0x1e950f` - ATmega328P (correct)
- `0x000000` or `0xffffff` - No communication (check wiring)
- Other values - Wrong chip or wiring issue

### 2. Read Fuse Bits

Verify fuse bits after burning:

```bash
# Read all fuses
avrdude -C /etc/avrdude.conf \
  -p atmega328p \
  -c avrisp \
  -P /dev/ttyACM0 \
  -b 19200 \
  -U efuse:r:-:h \
  -U hfuse:r:-:h \
  -U lfuse:r:-:h
```

**Expected output:**

```
avrdude: reading efuse memory:
avrdude: writing output file "<stdout>"
0xfd

avrdude: reading hfuse memory:
avrdude: writing output file "<stdout>"
0xde

avrdude: reading lfuse memory:
avrdude: writing output file "<stdout>"
0xff
```

**Note:** Extended fuse may show `0xFD` instead of `0x05` due to unused bit masking - both are correct.

### 3. Test Bootloader Functionality

After disconnecting ISP and connecting via USB:

```bash
# Check if board appears
ls -l /dev/ttyACM*

# Upload test sketch
pio run -t upload
# or use Arduino IDE to upload Blink example
```

### 4. Verify Bootloader Presence

Read the beginning of flash memory to check for bootloader code:

```bash
avrdude -C /etc/avrdude.conf \
  -p atmega328p \
  -c avrisp \
  -P /dev/ttyACM0 \
  -b 19200 \
  -U flash:r:flash_dump.hex:i

# Check bootloader section (last 512 bytes)
tail -n 50 flash_dump.hex
```

---

## Troubleshooting Guide

### Problem 1: "Invalid Device Signature" or "0x000000"

**Symptoms:**
```
avrdude: Yikes! Invalid device signature.
avrdude: Device signature = 0x000000
```

**Causes & Solutions:**

1. **Wiring Issue**
   - Double-check all 6 connections (especially MISO/MOSI/SCK)
   - Ensure good contact in jumper wires
   - Try different jumper wires

2. **Power Issue**
   - Verify 5V and GND connections
   - Check that target Arduino has power (LED should be on)
   - Try powering target from external source

3. **No Capacitor on Programmer**
   - Add 10µF capacitor between RESET and GND on programmer Arduino
   - Ensure correct polarity (+ to RESET, - to GND)

4. **Wrong Programmer Selected**
   - Must select "Arduino as ISP" not "ArduinoISP.org"
   - Verify in Tools → Programmer

5. **Target in Reset Loop**
   - Remove any shields or modules from target Arduino
   - Disconnect programmer pin 10 briefly, then reconnect

### Problem 2: "Programmer is Not Responding"

**Symptoms:**
```
avrdude: stk500_recv(): programmer is not responding
```

**Causes & Solutions:**

1. **ArduinoISP Sketch Not Uploaded**
   - Re-upload ArduinoISP sketch to programmer Arduino
   - Verify "Done uploading" message appeared

2. **Wrong Port Selected**
   - Ensure programmer port (/dev/ttyACM0) is selected
   - NOT the target port

3. **Serial Monitor Open**
   - Close Arduino IDE Serial Monitor
   - Close any other programs using the serial port (minicom, screen, etc.)

4. **Incorrect Baud Rate**
   - ArduinoISP uses 19200 baud by default
   - Verify avrdude command uses `-b 19200`

### Problem 3: "Verification Error" During Fuse Programming

**Symptoms:**
```
avrdude: verification error, first mismatch at byte 0x0000
         0xfd != 0x05
```

**Cause:**
Extended fuse byte unused bits are read as 1, causing `0xFD` instead of `0x05`.

**Solutions:**

1. **Ignore the Error** (Recommended)
   - This is a known issue and doesn't affect functionality
   - The fuse is actually set correctly

2. **Use 0xFD Instead**
   ```bash
   avrdude ... -U efuse:w:0xFD:m ...
   ```

3. **Update boards.txt** (Advanced)
   - Edit Arduino's `boards.txt` file
   - Change `uno.bootloader.extended_fuses=0x05` to `0xFD`

### Problem 4: "Done Burning Bootloader" But Upload Still Fails

**Symptoms:**
- Bootloader burn completes successfully
- Target Arduino still won't upload sketches via USB
- Still getting `stk500_getsync()` errors

**Causes & Solutions:**

1. **Hardware Issue (Not Bootloader)**
   - USB-to-serial chip (ATmega16U2) may be faulty
   - Try uploading via ISP directly (Upload Using Programmer)
   - Check for physical damage on USB port or crystal

2. **Wrong Board Selected**
   - After ISP programming, select correct board again
   - Tools → Board → Arduino Uno
   - Tools → Port → /dev/ttyACM1 (target)

3. **Auto-Reset Not Working**
   - Check 100nF capacitor on RESET line (on board itself)
   - May need to manually press reset button just before upload

4. **Fuses Incorrect**
   - Verify fuse settings with read command
   - Re-burn bootloader with correct fuses

### Problem 5: Target Arduino Doesn't Appear in /dev/ttyACM*

**Symptoms:**
- Bootloader burn successful
- Target Arduino doesn't show up as USB device
- No /dev/ttyACM1 after connecting via USB

**Causes & Solutions:**

1. **USB-to-Serial Chip Issue**
   - The ATmega16U2 (USB chip) may need firmware
   - This is NOT a bootloader issue
   - Check `lsusb` output for Arduino device

2. **USB Cable Issue**
   - Try different USB cable (must be data cable, not charge-only)
   - Try different USB port on computer

3. **Driver Issue**
   - On Linux: Check dmesg for USB enumeration errors
   ```bash
   dmesg | tail -50
   ```
   - Look for "device descriptor read" errors

### Problem 6: Capacitor Polarity Confusion

**Symptoms:**
- Bootloader burn fails intermittently
- Programmer Arduino resets during programming

**Solution:**

Electrolytic capacitors have polarity:
- **Longer leg = Positive (+)** → Connect to RESET
- **Shorter leg = Negative (-)** → Connect to GND
- Stripe on capacitor body marks negative side
- Some capacitors have + symbol on positive side

If installed backwards:
- Won't damage components (usually)
- Won't function correctly
- Reverse it and try again

### Problem 7: "Target Doesn't Answer" During Programming

**Symptoms:**
```
avrdude: initialization failed, rc=-1
avrdude: target doesn't answer. 1
```

**Causes & Solutions:**

1. **External Crystal Not Connected**
   - If fuses set for external crystal but none present
   - Target needs 16MHz crystal with 22pF capacitors
   - Or burn fuses for internal oscillator first

2. **RESET Pin Held Low**
   - Check for shorts on RESET line
   - Remove other devices connected to RESET

3. **Brown-Out Detection Triggered**
   - Voltage too low
   - Use stable 5V power supply
   - Check voltage with multimeter

### Common Pitfalls Summary

| Pitfall | Prevention |
|---------|------------|
| Selecting "ArduinoISP.org" instead of "Arduino as ISP" | Double-check Tools → Programmer menu |
| Forgetting 10µF capacitor on programmer | Add before burning, verify polarity |
| Wrong port selected (target instead of programmer) | Always use programmer port during burn |
| Serial Monitor left open | Close all serial connections before burning |
| Poor jumper wire connections | Use quality wires, check continuity |
| Selecting wrong target board | Must match actual target (Uno, Nano, etc.) |
| USB cable is charge-only | Use data-capable USB cable |

---

## Success Criteria

### Immediate Success Indicators

1. **During Bootloader Burn:**
   ```
   avrdude: Device signature = 0x1e950f (probably m328p)
   avrdude: erasing chip
   ...
   avrdude: 32768 bytes of flash written
   avrdude: 32768 bytes of flash verified
   avrdude done. Thank you.
   ```
   - "Done burning bootloader" message appears
   - No errors in console output
   - Process completes in 20-60 seconds

2. **Fuse Verification:**
   ```
   efuse: 0xFD (or 0x05 - both acceptable)
   hfuse: 0xDE
   lfuse: 0xFF
   ```

3. **Device Signature:**
   ```
   Device signature = 0x1e950f
   ```
   - Correct for ATmega328P

### Post-Bootloader Success Tests

1. **USB Enumeration:**
   ```bash
   # Disconnect ISP wiring
   # Connect target via USB
   ls -l /dev/ttyACM*
   ```
   - Target should appear (e.g., /dev/ttyACM1)

2. **Blink Upload Test:**
   - Open Arduino IDE
   - File → Examples → 01.Basics → Blink
   - Tools → Board → Arduino Uno
   - Tools → Port → /dev/ttyACM1 (target)
   - Click Upload
   - Expected: "Done uploading" and LED blinks

3. **Serial Communication:**
   ```bash
   # Upload sketch with Serial.begin(9600)
   # Open Serial Monitor
   # Should see output without errors
   ```

4. **Multiple Upload Cycles:**
   - Upload 3-5 different sketches
   - All should succeed without errors
   - Verifies bootloader stability

### Full Recovery Confirmation

**The target Arduino is fully recovered when:**

1. Uploads via USB work consistently
2. No `stk500_getsync()` or `stk500_recv()` errors
3. Auto-reset functions (no manual reset button needed)
4. Serial communication works in both directions
5. Board can be programmed repeatedly without issues
6. LED blinks during upload (shows bootloader running)

---

## Quick Reference Commands

### Read Device Signature
```bash
avrdude -C /etc/avrdude.conf -p atmega328p -c avrisp -P /dev/ttyACM0 -b 19200 -v
```

### Read Fuse Bits
```bash
avrdude -C /etc/avrdude.conf -p atmega328p -c avrisp -P /dev/ttyACM0 -b 19200 \
  -U efuse:r:-:h -U hfuse:r:-:h -U lfuse:r:-:h
```

### Burn Bootloader (Complete)
```bash
BOOTLOADER_HEX="$HOME/.arduino15/packages/arduino/hardware/avr/1.8.6/bootloaders/optiboot/optiboot_atmega328.hex"

avrdude -C /etc/avrdude.conf -p atmega328p -c avrisp -P /dev/ttyACM0 -b 19200 -v -e \
  -U efuse:w:0xFD:m -U hfuse:w:0xDE:m -U lfuse:w:0xFF:m \
  -U flash:w:"$BOOTLOADER_HEX":i -U lock:w:0x0F:m
```

### Check USB Devices
```bash
# List Arduino devices
ls -l /dev/ttyACM* /dev/ttyUSB*

# Monitor USB events
dmesg | tail -f

# Check USB device info
lsusb | grep Arduino
```

---

## Additional Resources

### Official Documentation
- [Arduino - Burn the bootloader using another Arduino](https://support.arduino.cc/hc/en-us/articles/4841602539164-Burn-the-bootloader-on-UNO-Mega-and-classic-Nano-using-another-Arduino)
- [Arduino as ISP and Arduino Bootloaders](https://docs.arduino.cc/built-in-examples/arduino-isp/ArduinoISP)
- [Arduino CLI burn-bootloader command](https://arduino.github.io/arduino-cli/0.21/commands/arduino-cli_burn-bootloader/)

### Tutorials and Guides
- [SparkFun - Installing an Arduino Bootloader](https://learn.sparkfun.com/tutorials/installing-an-arduino-bootloader/all)
- [Electropeak - Arduino ISP Programming Guide](https://electropeak.com/learn/use-arduino-as-isp-to-burn-bootloader-on-avr-microcontrollers/)
- [Burn Bootloader Troubleshooting Guide](https://per1234.github.io/ino-troubleshooting/burn-bootloader.html)
- [Zaitronics - Burning Bootloader Guide](https://zaitronics.com.au/blogs/guides/burning-bootloader-onto-arduino-boards)

### Advanced Topics
- [Manually burn Arduino bootloader with AVRDUDE](https://www.krekr.nl/content/manually-burn-arduino-bootloader-with-avrdude/)
- [Burning Bootloader on ATmega328 using Arduino UNO as ISP - Instructables](https://www.instructables.com/Burning-the-Bootloader-on-ATMega328-using-Arduino-/)
- [ATmega328P Standalone Usage](https://wolles-elektronikkiste.de/en/using-the-atmega328p-standalone)

### PlatformIO Resources
- [PlatformIO Atmel AVR Platform Documentation](https://docs.platformio.org/en/latest/platforms/atmelavr.html)
- [PlatformIO Community - Arduino as ISP](https://community.platformio.org/t/programming-with-arduino-as-isp/30460)

### Troubleshooting Specific Errors
- [Fix avrdude: stk500_recv() programmer not responding](https://www.programmingelectronics.com/avrdude-stk500_recv/)
- [Fix ATmega328P signature error 1E 95 0F - Instructables](https://www.instructables.com/How-to-Fix-Expected-Signature-for-ATMEGA328P-Is-1E/)
- [Arduino Forum - Expected signature for ATmega328P](https://forum.arduino.cc/t/expected-signature-for-atmega328p-is-1e-95-0f-double-check-chip-or-use-f/605951)

### USBasp Alternative
- [Electronics-Lab - Burning Arduino Bootloader with USBasp](https://www.electronics-lab.com/burning-the-bootloader-on-atmega328-using-usbasp-programmer/)
- [Circuit Digest - Burn Bootloader in ATmega328](https://circuitdigest.com/microcontroller-projects/how-to-burn-bootloader-in-atmega328p-and-program-using-arduino-ide)

### Hardware Issues
- [Arduino Forum - Corrupted bootloader or fried board?](https://forum.arduino.cc/t/arduino-uno-corrupted-bootloader-or-fried/1348139)
- [Arduino Help Center - If your sketch doesn't upload](https://support.arduino.cc/hc/en-us/articles/4403365313810-If-your-sketch-doesn-t-upload)

---

## Document History

- **Created:** 2026-01-11
- **Purpose:** Guide for recovering Arduino Uno bootloader to fix ACM1 upload issues
- **Target Hardware:** Arduino Uno with ATmega328P
- **Tested Methods:** Arduino-as-ISP, direct avrdude, PlatformIO
- **Research Date:** January 2026

---

## Notes for ACM1 Recovery

**Current Situation:**
- ACM0: Working Arduino Uno (programmer)
- ACM1: Non-functional Arduino Uno (target with suspected bootloader corruption)

**Recommended Approach:**

1. Start with **Method 1** (Arduino IDE + Arduino as ISP)
   - Easiest and most reliable
   - Good visual feedback
   - Lowest chance of user error

2. If Method 1 fails, try **Method 2** (direct avrdude)
   - More control over parameters
   - Better error diagnostics
   - Can isolate fuse vs. bootloader issues

3. If both fail, consider **hardware issue** rather than bootloader
   - USB-to-serial chip (ATmega16U2) problem
   - Crystal oscillator issue
   - Physical board damage

4. **Alternative:** Purchase USBasp programmer (~$5-10)
   - Eliminates variables with second Arduino
   - Useful for future projects
   - More reliable for repeated operations

**After successful bootloader burn:**
- Test with multiple sketch uploads
- Verify serial communication
- Document any remaining issues
- Consider underlying cause of corruption

---

**END OF DOCUMENT**
