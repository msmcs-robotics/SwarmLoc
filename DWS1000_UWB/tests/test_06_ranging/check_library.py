#!/usr/bin/env python3
"""
Check DW1000 library installation and test basic communication
"""

import serial
import time
import sys

def send_command_and_wait(ser, command, wait_time=2):
    """Send a command and wait for response"""
    print(f"Sending: {command}")
    ser.write(f"{command}\n".encode())
    time.sleep(wait_time)

    lines = []
    while ser.in_waiting > 0:
        line = ser.readline().decode('utf-8', errors='replace').strip()
        if line:
            print(f"  Response: {line}")
            lines.append(line)

    return lines

def reset_device(port, baud=115200):
    """Reset device by toggling DTR"""
    print(f"\nResetting device on {port}...")
    try:
        ser = serial.Serial(port, baud, timeout=1)
        ser.setDTR(False)
        time.sleep(0.5)
        ser.setDTR(True)
        time.sleep(2)

        # Read any startup messages
        print("Reading startup messages...")
        for i in range(50):  # Wait up to 5 seconds
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='replace').strip()
                if line:
                    print(f"  {line}")
            else:
                time.sleep(0.1)

        ser.close()
        return True
    except Exception as e:
        print(f"ERROR: {e}")
        return False

def monitor_device(port, duration=30, baud=115200):
    """Monitor a device for a specific duration"""
    print(f"\nMonitoring {port} for {duration} seconds...")
    try:
        ser = serial.Serial(port, baud, timeout=1)
        time.sleep(1)

        start_time = time.time()
        line_count = 0

        while (time.time() - start_time) < duration:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='replace').strip()
                if line:
                    print(f"  [{time.time() - start_time:.1f}s] {line}")
                    line_count += 1
            else:
                time.sleep(0.01)

        ser.close()
        print(f"Captured {line_count} lines")
        return line_count > 0

    except Exception as e:
        print(f"ERROR: {e}")
        return False

if __name__ == "__main__":
    ANCHOR_PORT = "/dev/ttyACM0"
    TAG_PORT = "/dev/ttyACM1"

    print("=" * 60)
    print("DW1000 Library Communication Test")
    print("=" * 60)

    # Step 1: Reset both devices
    print("\n[STEP 1] Resetting devices...")
    reset_device(ANCHOR_PORT)
    reset_device(TAG_PORT)

    # Step 2: Monitor for 30 seconds
    print("\n[STEP 2] Monitoring both devices for 30 seconds...")
    print("(Devices should be discovering each other and ranging)")

    import threading

    def monitor_anchor():
        monitor_device(ANCHOR_PORT, 30)

    def monitor_tag():
        monitor_device(TAG_PORT, 30)

    anchor_thread = threading.Thread(target=monitor_anchor)
    tag_thread = threading.Thread(target=monitor_tag)

    anchor_thread.start()
    tag_thread.start()

    anchor_thread.join()
    tag_thread.join()

    print("\n" + "=" * 60)
    print("Test complete")
    print("=" * 60)
