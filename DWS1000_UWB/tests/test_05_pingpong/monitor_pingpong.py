#!/usr/bin/env python3

"""
Test 05: MessagePingPong Serial Monitor

Purpose: Monitor both sender and receiver simultaneously
Date: 2026-01-11
"""

import serial
import time
import sys
import threading
from datetime import datetime

# Configuration
SENDER_PORT = "/dev/ttyACM0"
RECEIVER_PORT = "/dev/ttyACM1"
BAUD_RATE = 9600
MONITOR_DURATION = 180  # seconds (3 minutes)

# Output files
SENDER_OUTPUT = "/home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_05_pingpong/sender_output.txt"
RECEIVER_OUTPUT = "/home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_05_pingpong/receiver_output.txt"

# Global flag for monitoring
monitoring = True

# ANSI colors
GREEN = '\033[0;32m'
BLUE = '\033[0;34m'
RED = '\033[0;31m'
YELLOW = '\033[1;33m'
NC = '\033[0m'  # No Color


def monitor_device(port, device_name, output_file, color):
    """Monitor a single device"""
    global monitoring

    try:
        ser = serial.Serial(port, BAUD_RATE, timeout=1)
        time.sleep(2)  # Wait for device to be ready

        with open(output_file, 'w') as f:
            print(f"{color}[{device_name}] Monitoring started on {port}{NC}")
            f.write(f"=== {device_name} Output ===\n")
            f.write(f"Port: {port}\n")
            f.write(f"Baud: {BAUD_RATE}\n")
            f.write(f"Start Time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write("=" * 60 + "\n\n")

            line_count = 0

            while monitoring:
                if ser.in_waiting > 0:
                    try:
                        line = ser.readline().decode('utf-8', errors='replace').rstrip()
                        if line:
                            timestamp = datetime.now().strftime('%H:%M:%S.%f')[:-3]
                            output = f"[{timestamp}] {line}"

                            # Print to console with color
                            print(f"{color}[{device_name}] {line}{NC}")

                            # Write to file
                            f.write(output + "\n")
                            f.flush()

                            line_count += 1
                    except UnicodeDecodeError:
                        pass

                time.sleep(0.01)  # Small delay to prevent CPU spinning

            # Summary
            end_time = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
            f.write("\n" + "=" * 60 + "\n")
            f.write(f"End Time: {end_time}\n")
            f.write(f"Total Lines Captured: {line_count}\n")

            print(f"{color}[{device_name}] Monitoring stopped. {line_count} lines captured.{NC}")

        ser.close()

    except serial.SerialException as e:
        print(f"{RED}[{device_name}] ERROR: Could not open {port}: {e}{NC}")
    except Exception as e:
        print(f"{RED}[{device_name}] ERROR: {e}{NC}")


def main():
    global monitoring

    print(f"{BLUE}========================================{NC}")
    print(f"{BLUE}Test 05: MessagePingPong Monitor{NC}")
    print(f"{BLUE}========================================{NC}")
    print()
    print(f"Sender Port: {SENDER_PORT}")
    print(f"Receiver Port: {RECEIVER_PORT}")
    print(f"Baud Rate: {BAUD_RATE}")
    print(f"Duration: {MONITOR_DURATION} seconds")
    print()
    print(f"{YELLOW}Press Ctrl+C to stop early{NC}")
    print()

    # Start monitoring threads
    sender_thread = threading.Thread(
        target=monitor_device,
        args=(SENDER_PORT, "SENDER", SENDER_OUTPUT, GREEN)
    )

    receiver_thread = threading.Thread(
        target=monitor_device,
        args=(RECEIVER_PORT, "RECEIVER", RECEIVER_OUTPUT, BLUE)
    )

    sender_thread.daemon = True
    receiver_thread.daemon = True

    sender_thread.start()
    receiver_thread.start()

    try:
        # Monitor for specified duration
        for remaining in range(MONITOR_DURATION, 0, -1):
            time.sleep(1)
            if remaining % 30 == 0:  # Print every 30 seconds
                print(f"{YELLOW}Time remaining: {remaining} seconds...{NC}")

        print()
        print(f"{GREEN}Monitoring duration complete.{NC}")

    except KeyboardInterrupt:
        print()
        print(f"{YELLOW}Monitoring interrupted by user.{NC}")

    # Stop monitoring
    monitoring = False

    # Wait for threads to finish
    sender_thread.join(timeout=2)
    receiver_thread.join(timeout=2)

    print()
    print(f"{BLUE}========================================{NC}")
    print(f"{BLUE}Monitoring Complete{NC}")
    print(f"{BLUE}========================================{NC}")
    print()
    print(f"Output files:")
    print(f"  Sender:   {SENDER_OUTPUT}")
    print(f"  Receiver: {RECEIVER_OUTPUT}")
    print()

    # Analysis
    print(f"{YELLOW}Quick Analysis:{NC}")

    try:
        with open(SENDER_OUTPUT, 'r') as f:
            sender_lines = len([l for l in f if l.strip() and not l.startswith('=')])

        with open(RECEIVER_OUTPUT, 'r') as f:
            receiver_lines = len([l for l in f if l.strip() and not l.startswith('=')])

        print(f"  Sender lines: {sender_lines}")
        print(f"  Receiver lines: {receiver_lines}")

        if sender_lines > 5 and receiver_lines > 5:
            print(f"{GREEN}✓ Both devices appear to be communicating{NC}")
        elif sender_lines > 5 or receiver_lines > 5:
            print(f"{YELLOW}⚠ One device may not be communicating properly{NC}")
        else:
            print(f"{RED}✗ Little or no communication detected{NC}")

    except Exception as e:
        print(f"{RED}Could not analyze output: {e}{NC}")

    print()


if __name__ == "__main__":
    main()
