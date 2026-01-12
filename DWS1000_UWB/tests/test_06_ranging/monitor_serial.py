#!/usr/bin/env python3
"""
Serial Monitor for DW1000Ranging Test
Monitors both Anchor and Tag devices at 115200 baud
Captures output to files and displays in real-time
"""

import serial
import time
import sys
from datetime import datetime
import threading

class SerialMonitor:
    def __init__(self, port, device_name, output_file, baud_rate=115200):
        self.port = port
        self.device_name = device_name
        self.output_file = output_file
        self.baud_rate = baud_rate
        self.running = False
        self.serial_conn = None

    def connect(self):
        """Connect to serial port"""
        try:
            self.serial_conn = serial.Serial(
                port=self.port,
                baudrate=self.baud_rate,
                timeout=1,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE
            )
            time.sleep(2)  # Wait for connection to stabilize
            print(f"[{self.device_name}] Connected to {self.port} at {self.baud_rate} baud")
            return True
        except Exception as e:
            print(f"[{self.device_name}] ERROR: Failed to connect to {self.port}: {e}")
            return False

    def monitor(self, duration=None):
        """Monitor serial output"""
        if not self.serial_conn:
            print(f"[{self.device_name}] ERROR: Not connected")
            return

        self.running = True
        start_time = time.time()
        line_count = 0

        with open(self.output_file, 'w') as f:
            f.write(f"=== {self.device_name} Serial Monitor Log ===\n")
            f.write(f"Port: {self.port}\n")
            f.write(f"Baud Rate: {self.baud_rate}\n")
            f.write(f"Start Time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write("=" * 60 + "\n\n")

            try:
                while self.running:
                    if duration and (time.time() - start_time) > duration:
                        break

                    if self.serial_conn.in_waiting > 0:
                        try:
                            line = self.serial_conn.readline().decode('utf-8', errors='replace').rstrip()
                            if line:
                                timestamp = datetime.now().strftime('%H:%M:%S.%f')[:-3]
                                output_line = f"[{timestamp}] {line}"
                                print(f"[{self.device_name}] {output_line}")
                                f.write(output_line + "\n")
                                f.flush()
                                line_count += 1
                        except Exception as e:
                            print(f"[{self.device_name}] Read error: {e}")
                    else:
                        time.sleep(0.01)

            except KeyboardInterrupt:
                print(f"\n[{self.device_name}] Monitoring interrupted by user")
            finally:
                elapsed = time.time() - start_time
                f.write(f"\n" + "=" * 60 + "\n")
                f.write(f"End Time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
                f.write(f"Duration: {elapsed:.1f} seconds\n")
                f.write(f"Lines Captured: {line_count}\n")

        print(f"[{self.device_name}] Captured {line_count} lines in {elapsed:.1f} seconds")

    def close(self):
        """Close serial connection"""
        self.running = False
        if self.serial_conn and self.serial_conn.is_open:
            self.serial_conn.close()
            print(f"[{self.device_name}] Connection closed")


def monitor_both_devices(anchor_port, tag_port, duration=180):
    """Monitor both anchor and tag devices simultaneously"""

    # Create output directory
    import os
    output_dir = os.path.dirname(os.path.abspath(__file__))

    # Create monitors
    anchor = SerialMonitor(
        port=anchor_port,
        device_name="ANCHOR",
        output_file=os.path.join(output_dir, "anchor_output.txt")
    )

    tag = SerialMonitor(
        port=tag_port,
        device_name="TAG",
        output_file=os.path.join(output_dir, "tag_output.txt")
    )

    # Connect to devices
    print("Connecting to devices...")
    anchor_connected = anchor.connect()
    tag_connected = tag.connect()

    if not anchor_connected and not tag_connected:
        print("\nERROR: Could not connect to any device!")
        return False

    if not anchor_connected:
        print("\nWARNING: Anchor not connected, monitoring tag only")

    if not tag_connected:
        print("\nWARNING: Tag not connected, monitoring anchor only")

    print(f"\nStarting monitoring for {duration} seconds...")
    print("Press Ctrl+C to stop early\n")

    # Start monitoring in separate threads
    threads = []

    if anchor_connected:
        anchor_thread = threading.Thread(target=anchor.monitor, args=(duration,))
        anchor_thread.start()
        threads.append(anchor_thread)

    if tag_connected:
        tag_thread = threading.Thread(target=tag.monitor, args=(duration,))
        tag_thread.start()
        threads.append(tag_thread)

    # Wait for threads to complete
    try:
        for thread in threads:
            thread.join()
    except KeyboardInterrupt:
        print("\n\nStopping monitoring...")
        anchor.running = False
        tag.running = False
        for thread in threads:
            thread.join(timeout=2)

    # Close connections
    anchor.close()
    tag.close()

    print("\n" + "=" * 60)
    print("Monitoring complete!")
    print(f"Anchor output: {output_dir}/anchor_output.txt")
    print(f"Tag output: {output_dir}/tag_output.txt")
    print("=" * 60)

    return True


if __name__ == "__main__":
    # Default settings
    ANCHOR_PORT = "/dev/ttyACM0"
    TAG_PORT = "/dev/ttyACM1"
    DURATION = 180  # 3 minutes

    # Parse command line arguments
    if len(sys.argv) > 1:
        DURATION = int(sys.argv[1])

    if len(sys.argv) > 2:
        ANCHOR_PORT = sys.argv[2]

    if len(sys.argv) > 3:
        TAG_PORT = sys.argv[3]

    print("=" * 60)
    print("DW1000Ranging Serial Monitor")
    print("=" * 60)
    print(f"Anchor Port: {ANCHOR_PORT}")
    print(f"Tag Port: {TAG_PORT}")
    print(f"Baud Rate: 115200")
    print(f"Duration: {DURATION} seconds")
    print("=" * 60 + "\n")

    success = monitor_both_devices(ANCHOR_PORT, TAG_PORT, DURATION)

    sys.exit(0 if success else 1)
