# DWS1000_UWB - Known Issues

> Quick reference for recurring problems. Check here first when things go wrong.

## USB / Upload Issues

### Arduino won't upload (stk500_getsync errors)

**Symptoms:** `avrdude: stk500_getsync(): not in sync` during PlatformIO upload.

**Check these in order:**

1. **USB port / hub contention.** When both Arduinos are on the same USB bus (e.g., both on right-side laptop ports = Bus 3), the secondary device often fails uploads. This is a known Arduino bootloader timing issue with USB hubs. Check topology:
   ```bash
   udevadm info -q property --name=/dev/ttyACM0 | grep ID_PATH
   ```
   **Software fix (try first):** USB device reset via kernel sysfs — this re-enumerates the device and often fixes the timing:
   ```bash
   # Find USB device (e.g., 3-5 from the ID_PATH)
   USB_DEV="3-5"
   echo 0 | sudo tee /sys/bus/usb/devices/$USB_DEV/authorized
   sleep 2
   echo 1 | sudo tee /sys/bus/usb/devices/$USB_DEV/authorized
   sleep 3
   # Then retry upload
   ```
   **Hardware fix:** Move one Arduino to a different USB bus (e.g., left-side laptop port vs right-side). Having Arduinos on separate USB buses avoids contention entirely.

2. **STM32CubeIDE (or similar) may claim the port.** If STM32CubeIDE is running, it can hold serial ports open even when no STM32 board is connected.
   ```bash
   ps aux | grep -i stm32  # Check if running
   ```
   **Fix:** Close STM32CubeIDE before uploading to Arduino.

3. **Port renumbered after replug.** After unplugging/replugging, ACM0 may become ACM1 or ACM2.
   ```bash
   ls -la /dev/ttyACM*
   ```
   **Fix:** Check which port is which using `udevadm info` and use the correct port number.

4. **Reset timing.** Sometimes the Arduino bootloader just misses the sync window.
   **Fix:** Try uploading again (2-3 attempts). Press the reset button on the Arduino right before upload starts.

### Which device is on which port?

Devices have different serial numbers. Identify with:
```bash
for dev in /dev/ttyACM*; do
  echo "$dev: $(udevadm info -q property --name=$dev | grep ID_SERIAL=)"
done
```
Known serials:
- `...9351E0A001` = DEV0 (LDO 0x88, better receiver)
- `...51306142` = DEV1 (LDO 0x28)

## SPI / Communication Issues

### All register reads return 0xFF

**Cause:** SPI_EDGE_BIT (bit 10 of SYS_CFG) is incompatible with AVR SPI hardware.

**Fix:** Already patched in `lib/DW1000/src/DW1000.cpp` with `#if !defined(__AVR__)` guard. If using a fresh library copy, re-apply this fix. See [SPI_EDGE_FIX_SESSION_2026-02-12.md](findings/SPI_EDGE_FIX_SESSION_2026-02-12.md).

### SPI reads unreliable during RX mode

**Cause:** EMI from DW1000 radio front-end corrupts SPI during active receive. IDLE=100%, TX=100%, RX=75-90%.

**Mitigations:**
- Poll the IRQ pin (digital read, no SPI) instead of SPI status registers during RX
- Use double-read with retry for any SPI reads during RX
- Watchdog timer to restart receiver if stuck

## DW1000 Configuration Issues

### PLL not locking / Clock problem errors

**Cause:** LDO tuning from OTP not applied, or overwritten by `commitConfiguration()`.

**Fix:** Always re-apply LDO tuning AFTER `commitConfiguration()`. The `tune()` method reconfigures PLL and overwrites LDO settings.
```cpp
DW1000.commitConfiguration();
applyLDOTuning();  // Must come after
```

### False error interrupts (PLL LL sticky bits)

**Cause:** `isClockProblem()` checks sticky bits that may be set from transient events during config changes. These are not real errors.

**Fix:** Already patched — `isClockProblem()` moved to end of `handleInterrupt()` in DW1000.cpp. Don't attach error handlers unless needed; they fire on these sticky bits.

## Hardware Notes

- **J1 jumper:** Leave OPEN (no jumper). DC-DC powers DWM1000 directly.
- **D8→D2 wire:** Required on each shield for IRQ routing.
- **LDO values:** DEV0=0x88, DEV1=0x28 (read from OTP address 0x04).

---

*Update when new issues are discovered.*
