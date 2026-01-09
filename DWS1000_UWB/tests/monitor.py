#!/usr/bin/env python3
"""
Serial Monitor for DWM3000 Testing

Monitors serial output from Arduino devices and logs to file.
Supports monitoring one or two devices simultaneously.

Usage:
    python monitor.py /dev/ttyACM0                    # Monitor one device
    python monitor.py /dev/ttyACM0 /dev/ttyACM1       # Monitor both devices
    python monitor.py --help
"""

import serial
import sys
import time
import argparse
from datetime import datetime
import threading

class SerialMonitor:
    def __init__(self, port, baudrate=9600, name="Device", log_file=None):
        self.port = port
        self.baudrate = baudrate
        self.name = name
        self.log_file = log_file
        self.running = False
        self.ser = None

    def connect(self):
        """Connect to serial port"""
        try:
            self.ser = serial.Serial(self.port, self.baudrate, timeout=1)
            print(f"[{self.name}] Connected to {self.port} at {self.baudrate} baud")
            return True
        except serial.SerialException as e:
            print(f"[{self.name}] Error connecting to {self.port}: {e}")
            return False

    def monitor(self):
        """Monitor serial output"""
        if not self.ser:
            print(f"[{self.name}] Not connected")
            return

        self.running = True
        print(f"[{self.name}] Monitoring started... (Ctrl+C to stop)")
        print("-" * 60)

        try:
            while self.running:
                if self.ser.in_waiting > 0:
                    line = self.ser.readline().decode('utf-8', errors='replace').rstrip()
                    timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]

                    # Format output
                    output = f"[{timestamp}] [{self.name}] {line}"
                    print(output)

                    # Log to file if specified
                    if self.log_file:
                        with open(self.log_file, 'a') as f:
                            f.write(output + '\n')

        except KeyboardInterrupt:
            print(f"\n[{self.name}] Monitoring stopped by user")
        except Exception as e:
            print(f"\n[{self.name}] Error: {e}")
        finally:
            self.stop()

    def stop(self):
        """Stop monitoring and close port"""
        self.running = False
        if self.ser and self.ser.is_open:
            self.ser.close()
            print(f"[{self.name}] Port closed")

def monitor_dual(port1, port2, baudrate=9600, log_file=None):
    """Monitor two serial ports simultaneously"""
    mon1 = SerialMonitor(port1, baudrate, "Initiator", log_file)
    mon2 = SerialMonitor(port2, baudrate, "Responder", log_file)

    if not mon1.connect() or not mon2.connect():
        print("Failed to connect to one or both devices")
        return

    # Create threads for each monitor
    thread1 = threading.Thread(target=mon1.monitor, daemon=True)
    thread2 = threading.Thread(target=mon2.monitor, daemon=True)

    thread1.start()
    thread2.start()

    try:
        # Wait for threads
        thread1.join()
        thread2.join()
    except KeyboardInterrupt:
        print("\nStopping monitors...")
        mon1.stop()
        mon2.stop()

def main():
    parser = argparse.ArgumentParser(
        description='Monitor serial output from DWM3000 test devices',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  %(prog)s /dev/ttyACM0
  %(prog)s /dev/ttyACM0 /dev/ttyACM1
  %(prog)s /dev/ttyACM0 --baud 115200
  %(prog)s /dev/ttyACM0 /dev/ttyACM1 --log test_results.log
        '''
    )

    parser.add_argument('port1', help='First serial port (e.g., /dev/ttyACM0)')
    parser.add_argument('port2', nargs='?', help='Second serial port for dual monitoring')
    parser.add_argument('-b', '--baud', type=int, default=9600,
                       help='Baud rate (default: 9600)')
    parser.add_argument('-l', '--log', help='Log file to save output')

    args = parser.parse_args()

    # Create log file with timestamp if logging enabled
    log_file = None
    if args.log:
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        log_file = args.log.replace('.log', f'_{timestamp}.log')
        print(f"Logging to: {log_file}\n")

    if args.port2:
        # Dual monitor mode
        monitor_dual(args.port1, args.port2, args.baud, log_file)
    else:
        # Single monitor mode
        mon = SerialMonitor(args.port1, args.baud, "Device", log_file)
        if mon.connect():
            mon.monitor()

if __name__ == '__main__':
    main()
