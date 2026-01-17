# DW1000 Library Code Review and Software Fixes

> Date: 2026-01-17
> Status: Comprehensive review completed
> Keywords: DW1000, library, initialization, PLL, CPLOCK, RFPLL_LL, software fix

## Executive Summary

A comprehensive code review of the DW1000 library identified several software issues that may contribute to PLL instability. While the root cause appears to be hardware power noise, these software improvements may help marginally and are worth implementing.

**Key finding:** The DW1000-ng library has better PLL stability features and should be considered as an alternative.

---

## 1. Critical Issues Found in DW1000 Library

### 1.1 Missing CPLL Lock Detect Configuration

**Location:** `lib/DW1000/src/DW1000.cpp` lines 119-157

The library does NOT enable CPLL lock detect (PLLLDT bit in EXT_SYNC:EC_CTRL register). This feature helps monitor PLL stability.

**Fix:** Add after initialization:
```cpp
// Enable CPLL lock detect
byte ecctrl[4];
DW1000.readBytes(0x24, 0x00, ecctrl, 4);
ecctrl[0] |= 0x04;  // Set PLLLDT bit
DW1000.writeBytes(0x24, 0x00, ecctrl, 4);
```

### 1.2 Crystal Trim Not Applied

**Location:** `lib/DW1000/src/DW1000.cpp` lines 677-685

The library reads XTAL trim from OTP but applies it too late (in `tune()` function). DW1000-ng applies it during initialization for better stability.

**Fix:** Apply XTAL trim early in initialization:
```cpp
byte buf_otp[4];
DW1000.readBytesOTP(0x01E, buf_otp);
byte fsxtalt;
if (buf_otp[0] == 0) {
    fsxtalt = 0x70;  // midrange (0x10) | 0x60
} else {
    fsxtalt = (buf_otp[0] & 0x1F) | 0x60;
}
DW1000.writeBytes(0x2B, 0x0E, &fsxtalt, 1);
```

### 1.3 Wrong PLL Config Write Order

**Location:** `lib/DW1000/src/DW1000.cpp` lines 702-704

Current order:
1. FS_PLLTUNE
2. FS_PLLCFG
3. FS_XTALT

Per Decawave Application Note APS011, correct order should be:
1. FS_PLLCFG first
2. FS_PLLTUNE second

### 1.4 No CPLOCK Verification

The library doesn't verify that CPLOCK is set before proceeding. It assumes 5ms delay is sufficient.

**Fix:** Add verification loop:
```cpp
// Wait for CPLOCK with timeout
for(int i = 0; i < 100; i++) {
    byte status[1];
    DW1000.readBytes(SYS_STATUS, 0x00, status, 1);
    if(status[0] & 0x02) {  // CPLOCK bit
        break;
    }
    delay(1);
}
```

### 1.5 LDO Tune Not Loaded from OTP

**Location:** `lib/DW1000/src/DW1000.cpp` lines 189-191

There's a TODO comment indicating LDO tune loading was never implemented:
```cpp
if(ldoTune[0] != 0) {
    // TODO tuning available, copy over to RAM: use OTP_LDO bit
}
```

---

## 2. DW1000 vs DW1000-ng Comparison

| Feature | DW1000 (Current) | DW1000-ng |
|---------|------------------|-----------|
| CPLL Lock Detect Enable | **Missing** | Implemented |
| XTAL Trim from OTP | Late/incomplete | Early init |
| Slow SPI during init | No | Yes (2MHz) |
| 5µs SPI transaction delays | No | Yes |
| Auto receiver reset on failure | No | Yes |
| Clock problem check in ISR | Manual | Automatic |
| LDO Tune from OTP | TODO (not done) | Implemented |
| PLL Config Order | Wrong | Correct |

**Recommendation:** Consider switching to DW1000-ng library.

---

## 3. Software Workarounds for PLL Stability

### 3.1 SPI Speed Adjustment

**Critical:** Use ≤3 MHz SPI during initialization, then switch to higher speeds after PLL locks.

```cpp
// During init
SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));  // 2 MHz

// After commitConfiguration
SPI.beginTransaction(SPISettings(16000000, MSBFIRST, SPI_MODE0));  // 16 MHz
```

### 3.2 PLL Error Recovery Handler

```cpp
void handlePLLError() {
    byte status[4];
    DW1000.readBytes(SYS_STATUS, 0x00, status, 4);

    // Check CLKPLL_LL (bit 25) or RFPLL_LL (bit 24)
    if (status[3] & 0x03) {
        // Disable transceiver
        DW1000.idle();

        // Clear PLL error flags
        byte clear[4] = {0, 0, 0, 0x03};
        DW1000.writeBytes(SYS_STATUS, 0x00, clear, 4);

        // Perform soft reset
        delay(10);
        DW1000.reset();

        // Reinitialize
        DW1000.begin(PIN_IRQ, PIN_RST);
        DW1000.select(PIN_SS);
        // ... reconfigure ...
    }
}
```

### 3.3 Receiver Reset After Error

```cpp
void resetReceiver() {
    byte softreset;
    softreset = 0xE0;  // Clear RX reset bit
    DW1000.writeBytes(0x36, 0x03, &softreset, 1);
    softreset = 0xF0;  // Set all reset bits
    DW1000.writeBytes(0x36, 0x03, &softreset, 1);
}
```

### 3.4 Wait After Reset Before SPI

```cpp
void safeInit() {
    // Reset
    pinMode(PIN_RST, OUTPUT);
    digitalWrite(PIN_RST, LOW);
    delay(2);
    pinMode(PIN_RST, INPUT);

    // Wait 10µs minimum before SPI access
    delayMicroseconds(10);

    // Now safe to access SPI at low speed
}
```

---

## 4. Register Settings for PLL Stability

### 4.1 FS_XTALT (Crystal Trim) - Register 0x2B:0E

```cpp
// Default 0x00 = ±30 ppm - NOT recommended
// Mid-range 0x10 = better stability
#define FS_XTALT_MIDRANGE 0x10
byte xtalt = 0x60 | (FS_XTALT_MIDRANGE & 0x1F);
```

### 4.2 Channel 5 PLL Configuration

```cpp
// FS_PLLCFG (0x2B:07) = 0x0800041D
// FS_PLLTUNE (0x2B:0B) = 0xBE (NOT default 0x46!)
```

### 4.3 EC_CTRL (PLL Lock Detect) - Register 0x24:00

```cpp
// Bit 2 (PLLLDT) = 1 to enable PLL lock detect
byte ecctrl = 0x04;
```

---

## 5. Missing Constants to Add

```cpp
// Add to DW1000Constants.h

// EXT_SYNC register for PLL lock detect
#define EXT_SYNC 0x24
#define EC_CTRL_SUB 0x00
#define LEN_EC_CTRL 4
#define PLLLDT_BIT 2

// PMSC soft reset sub-register
#define PMSC_SOFTRESET_SUB 0x03
#define LEN_PMSC_SOFTRESET 1

// Additional clock modes
#define LDE_CLOCK 0x03
#define TX_PLL_CLOCK 0x20
```

---

## 6. Conclusion

### Hardware Issue (Primary)
The RFPLL_LL flag indicates the RF PLL is losing lock due to power supply noise. This is a hardware issue that software cannot fully resolve.

### Software Improvements (Secondary)
The following software changes may help marginally:

1. **Enable CPLL lock detect** - Better monitoring
2. **Apply XTAL trim early** - Better clock stability
3. **Fix PLL config write order** - Per Decawave spec
4. **Add CPLOCK verification** - Don't proceed without lock
5. **Lower SPI speed during init** - Reduce noise
6. **Add PLL error recovery** - Automatic reset on failure

### Recommendation
**Try DW1000-ng library** which has these fixes already implemented.

---

## Sources

- DW1000 Library Code Review (agent analysis)
- [Qorvo Forum: CLKPLL_LL issues](https://forum.qorvo.com/t/dw1000-cant-receive-at-all-due-to-clkpll-ll-being-set-constantly/13862)
- [GitHub: arduino-dw1000 Issue #42](https://github.com/thotro/arduino-dw1000/issues/42)
- [DW1000 User Manual v2.17](https://www.sunnywale.com/uploadfile/2021/1230/DW1000%20User%20Manual_Awin.pdf)
- [GitHub: F-Army/arduino-dw1000-ng](https://github.com/F-Army/arduino-dw1000-ng)
- Decawave Application Note APS011
