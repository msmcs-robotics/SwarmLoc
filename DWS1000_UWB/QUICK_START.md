# Quick Start Guide - DW1000 Ranging Test

**Date**: 2026-01-11
**Status**: Ready for ranging test
**Expected Distance**: 45.72 cm (18 inches)

---

## ⚡ Quick Start (Cable Swap Method)

You have 2 Arduino Unos but `/dev/ttyACM1` has upload issues. Use this method:

### Step 1: Upload Both Devices (5 minutes)

```bash
cd /home/devel/Desktop/SwarmLoc/DWS1000_UWB
./upload_both_cable_swap.sh
```

The script will guide you through:
1. Upload ANCHOR firmware to first Arduino on ACM0
2. Unplug first Arduino, plug in second Arduino to ACM0
3. Upload TAG firmware to second Arduino

### Step 2: Connect Both Devices (1 minute)

- Plug in BOTH Arduinos (any USB ports - ACM0, ACM1, or anything)
- Position them **45.72 cm (18 inches)** apart

### Step 3: Start Ranging (1 minute)

Open TWO terminal windows:

**Terminal 1** (ANCHOR):
```bash
pio device monitor --port /dev/ttyACM0 --baud 115200
```

**Terminal 2** (TAG):
```bash
pio device monitor --port /dev/ttyACM1 --baud 115200
```

Both will show:
```
>>> Send any character to start ranging <<<
```

**In each terminal**: Type any letter and press Enter

### Step 4: Observe Results! 🎉

You should see:
```
[RANGE] 0.48 m (48 cm) | Error: +2.28 cm | from 3B9C
[RANGE] 0.45 m (45 cm) | Error: -0.72 cm | from 3B9C
[RANGE] 0.46 m (46 cm) | Error: +0.28 cm | from 3B9C
```

**Expected**: 40-55 cm (±10-15 cm error is normal before calibration)

---

## 📊 After First Test

### Collect Data
- Record 50+ measurements
- Calculate average distance
- Note the error from expected 45.72 cm

### Calibrate (if needed)
See: [ANTENNA_DELAY_CALIBRATION_2026.md](docs/findings/ANTENNA_DELAY_CALIBRATION_2026.md)

### Test Multiple Distances
- 0.5m, 1m, 2m, 3m, 5m
- Document accuracy at each distance

---

## 🔧 Troubleshooting

### No output in serial monitor?
- Check baud rate is 115200
- Press reset button on Arduino
- Verify USB cable is connected

### Devices don't detect each other?
- Make sure you sent "start" to BOTH devices
- Check addresses match in firmware
- Verify DW1000 shields are properly connected

### Wrong distance readings?
- Normal before calibration (±10-15 cm error)
- Follow calibration guide to improve accuracy

---

## 📚 Documentation

- **Project Status**: [PROJECT_STATUS.md](PROJECT_STATUS.md)
- **Full Roadmap**: [docs/roadmap.md](docs/roadmap.md)
- **Session Summary**: [docs/SESSION_FINAL_2026-01-11.md](docs/SESSION_FINAL_2026-01-11.md)
- **Upload Troubleshooting**: [docs/findings/UPLOAD_ISSUE_RESOLUTION.md](docs/findings/UPLOAD_ISSUE_RESOLUTION.md)

---

**Ready to start!** Run `./upload_both_cable_swap.sh` 🚀
