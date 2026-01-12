# Quick Start Guide - DW1000 Calibration

## 5-Minute Setup

### Prerequisites

```bash
# Check Python 3 is installed
python3 --version

# Install matplotlib (optional, for plots)
pip3 install matplotlib numpy
```

### Step 1: Upload Firmware (2 minutes)

1. Open `../../tests/calibration/calibration_test.ino` in Arduino IDE
2. For TAG device:
   ```cpp
   const bool IS_TAG = true;
   uint16_t ANTENNA_DELAY = 16450;
   ```
3. Upload to TAG
4. For ANCHOR device:
   ```cpp
   const bool IS_TAG = false;
   uint16_t ANTENNA_DELAY = 16450;
   ```
5. Upload to ANCHOR

### Step 2: Position Devices (1 minute)

- Place TAG and ANCHOR exactly **1.000 meters** apart
- Measure center-to-center (antenna to antenna)
- Ensure clear line-of-sight

### Step 3: Run Calibration (5-10 minutes)

```bash
# Make scripts executable (first time only)
chmod +x *.sh

# Run calibration
./calibrate_antenna_delay.sh /dev/ttyUSB0 1.0
```

**Note:** Replace `/dev/ttyUSB0` with your TAG's serial port

### Step 4: Follow Prompts

The script will:
1. Collect measurements (30 seconds)
2. Show error and suggested antenna delay
3. Prompt you to update firmware
4. Repeat until error < 5 cm

**Typical iterations: 2-4**

### Step 5: Validate (10 minutes)

```bash
./multi_distance_validation.sh /dev/ttyUSB0
```

Follow prompts to test at each distance.

## Expected Results

After calibration:
- Error: **< 5 cm** at all distances
- Std Dev: **< 2 cm**
- Final antenna delay: **16450-16500** (typical)

## Common Issues

### "Device not found"
```bash
# List available ports
ls -la /dev/ttyUSB* /dev/ttyACM*

# Fix permissions
sudo usermod -a -G dialout $USER
# Then logout and login
```

### "No measurements collected"
- Check both devices are powered
- Verify firmware is uploaded correctly
- Open serial monitor to see if CSV data is streaming

### "Calibration won't converge"
- Double-check measured distance is accurate
- Ensure both devices have SAME antenna delay
- Try different location (less RF interference)

## Test the Analysis Tool

```bash
# Test with sample data
python3 analyze_measurements.py \
    --input sample_data.csv \
    --actual-distance 1.0

# Expected output: Error ~8.8 cm, Adjustment +9
```

## Directory Structure After Calibration

```
calibration_data/
├── calibration_20260111_143022.log
├── iter_1_delay_16450.csv
├── iter_1_delay_16450.json
├── iter_2_delay_16459.csv
├── iter_2_delay_16459.json
└── calibration_report_20260111_143022.txt

validation_data/
└── validation_20260111_150000/
    ├── distance_0.5m.csv
    ├── distance_1.0m.csv
    ├── ...
    └── validation_report.txt
```

## Next Steps

1. **Document your calibration:**
   - Note final antenna delay value
   - Record date and environment
   - Label devices with serial numbers

2. **Use calibrated values in your application:**
   ```cpp
   // In your ranging application
   DW1000.setAntennaDelay(16459);  // Your calibrated value
   ```

3. **Periodic recalibration:**
   - Recalibrate if temperature changes >10°C
   - Or every 6 months for critical applications

## Full Documentation

See [README.md](README.md) for complete documentation.

---

**Estimated Total Time:** 15-20 minutes per device pair
