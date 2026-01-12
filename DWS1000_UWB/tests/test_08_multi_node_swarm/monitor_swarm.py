#!/usr/bin/env python3
"""
monitor_swarm.py - Multi-Device Serial Monitor for Swarm Testing

Project: SwarmLoc Multi-Node Swarm Test
Purpose: Monitor multiple Arduino nodes simultaneously with color-coded output

Features:
- Monitors all connected Arduino devices
- Color-coded output per node
- Logs to separate files per node
- Real-time ranging matrix display
- Detects issues (missed slots, collisions, lost nodes)
- Statistics tracking
- CSV data extraction

Usage:
    python3 monitor_swarm.py                    # Monitor all devices
    python3 monitor_swarm.py --devices /dev/ttyACM0 /dev/ttyACM1
    python3 monitor_swarm.py --matrix           # Show ranging matrix
    python3 monitor_swarm.py --help

Requirements:
    pip3 install pyserial colorama
"""

import sys
import serial
import serial.tools.list_ports
import threading
import queue
import time
import argparse
import re
import os
from datetime import datetime
from collections import defaultdict

try:
    from colorama import init, Fore, Back, Style
    init(autoreset=True)
    HAS_COLORAMA = True
except ImportError:
    print("WARNING: colorama not installed. Output will not be colored.")
    print("Install with: pip3 install colorama")
    HAS_COLORAMA = False
    # Dummy color definitions
    class Fore:
        RED = GREEN = YELLOW = BLUE = MAGENTA = CYAN = WHITE = RESET = ''
    class Style:
        BRIGHT = RESET_ALL = ''

# Node color mapping
NODE_COLORS = {
    1: Fore.GREEN,
    2: Fore.CYAN,
    3: Fore.YELLOW,
    4: Fore.MAGENTA,
    5: Fore.BLUE,
}

# Global data structures
range_matrix = defaultdict(lambda: defaultdict(lambda: {"distance": 0.0, "timestamp": 0}))
node_stats = defaultdict(lambda: {"ranges": 0, "errors": 0, "last_seen": 0})
log_files = {}

# Thread-safe output queue
output_queue = queue.Queue()


def detect_arduino_devices():
    """Detect all connected Arduino devices."""
    devices = []
    ports = serial.tools.list_ports.comports()

    for port in ports:
        # Look for Arduino devices
        if 'Arduino' in port.description or 'ttyACM' in port.device or 'ttyUSB' in port.device:
            devices.append(port.device)

    return devices


def parse_node_id_from_output(line):
    """Extract node ID from serial output."""
    # Look for "Node ID: X" in output
    match = re.search(r'Node ID:\s*(\d+)', line)
    if match:
        return int(match.group(1))

    # Look for CSV format: timestamp,node_id,target_id,distance,rx_power
    if line.strip() and line[0].isdigit():
        parts = line.strip().split(',')
        if len(parts) >= 2:
            try:
                return int(parts[1])
            except ValueError:
                pass

    return None


def parse_range_data(line):
    """Parse CSV range data: timestamp,node_id,target_id,distance,rx_power"""
    try:
        parts = line.strip().split(',')
        if len(parts) >= 5:
            timestamp = int(parts[0])
            node_id = int(parts[1])
            target_id = parts[2]  # May be hex
            distance = float(parts[3])
            rx_power = float(parts[4])
            return {
                'timestamp': timestamp,
                'node_id': node_id,
                'target_id': target_id,
                'distance': distance,
                'rx_power': rx_power
            }
    except (ValueError, IndexError):
        pass
    return None


def monitor_device(device, node_id=None):
    """Monitor a single device in a separate thread."""
    try:
        ser = serial.Serial(device, 115200, timeout=1)
        print(f"Connected to {device}")

        # Determine node ID if not provided
        if node_id is None:
            print(f"Waiting for node ID from {device}...")
            for _ in range(50):  # Try for 5 seconds
                if ser.in_waiting:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    detected_id = parse_node_id_from_output(line)
                    if detected_id:
                        node_id = detected_id
                        print(f"Detected Node {node_id} on {device}")
                        break
                time.sleep(0.1)

        if node_id is None:
            print(f"WARNING: Could not detect node ID for {device}, using 0")
            node_id = 0

        # Open log file
        log_dir = "logs"
        os.makedirs(log_dir, exist_ok=True)
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        log_filename = f"{log_dir}/node_{node_id}_{timestamp}.log"
        log_files[node_id] = open(log_filename, 'w')
        print(f"Logging Node {node_id} to {log_filename}")

        # Read loop
        while True:
            if ser.in_waiting:
                line = ser.readline().decode('utf-8', errors='ignore').strip()

                if line:
                    # Update stats
                    node_stats[node_id]['last_seen'] = time.time()

                    # Parse range data
                    range_data = parse_range_data(line)
                    if range_data:
                        node_stats[node_id]['ranges'] += 1
                        # Update ranging matrix
                        range_matrix[range_data['node_id']][range_data['target_id']] = {
                            'distance': range_data['distance'],
                            'timestamp': time.time()
                        }

                    # Check for errors
                    if 'ERROR' in line or 'DISCONNECT' in line:
                        node_stats[node_id]['errors'] += 1

                    # Write to log
                    log_files[node_id].write(f"{line}\n")
                    log_files[node_id].flush()

                    # Queue for display
                    color = NODE_COLORS.get(node_id, Fore.WHITE)
                    output_queue.put((node_id, color, line))

            time.sleep(0.01)

    except serial.SerialException as e:
        print(f"ERROR: Failed to open {device}: {e}")
    except KeyboardInterrupt:
        pass
    finally:
        if node_id in log_files:
            log_files[node_id].close()
        try:
            ser.close()
        except:
            pass


def display_output():
    """Display output from all devices with color coding."""
    try:
        while True:
            try:
                node_id, color, line = output_queue.get(timeout=1)
                print(f"{color}[Node {node_id}]{Style.RESET_ALL} {line}")
            except queue.Empty:
                pass
    except KeyboardInterrupt:
        pass


def display_ranging_matrix():
    """Display real-time ranging matrix."""
    while True:
        try:
            time.sleep(2)  # Update every 2 seconds

            # Clear screen (optional)
            # print("\033[2J\033[H")

            print("\n" + "="*60)
            print("RANGING MATRIX (distances in meters)")
            print("="*60)

            # Get all node IDs
            all_nodes = set()
            for src in range_matrix:
                all_nodes.add(src)
                for tgt in range_matrix[src]:
                    try:
                        all_nodes.add(int(tgt, 16) if isinstance(tgt, str) else tgt)
                    except:
                        pass

            if not all_nodes:
                print("No ranging data yet...")
                continue

            sorted_nodes = sorted(all_nodes)

            # Print header
            print(f"{'From\\To':<10}", end='')
            for node in sorted_nodes:
                print(f"{node:<10}", end='')
            print()

            # Print matrix
            current_time = time.time()
            for src_node in sorted_nodes:
                print(f"{src_node:<10}", end='')
                for tgt_node in sorted_nodes:
                    if src_node == tgt_node:
                        print(f"{'---':<10}", end='')
                    else:
                        # Look for range data
                        data = range_matrix[src_node].get(str(tgt_node))
                        if not data:
                            data = range_matrix[src_node].get(hex(tgt_node))

                        if data and (current_time - data['timestamp'] < 5):
                            distance = data['distance']
                            print(f"{distance:<10.2f}", end='')
                        else:
                            print(f"{'---':<10}", end='')
                print()

            # Print statistics
            print("\n" + "="*60)
            print("NODE STATISTICS")
            print("="*60)
            for node_id in sorted(node_stats.keys()):
                stats = node_stats[node_id]
                age = current_time - stats['last_seen']
                status = "ACTIVE" if age < 5 else "INACTIVE"
                color = Fore.GREEN if age < 5 else Fore.RED
                print(f"{color}Node {node_id}: {status} | "
                      f"Ranges: {stats['ranges']} | "
                      f"Errors: {stats['errors']}{Style.RESET_ALL}")

        except KeyboardInterrupt:
            break


def main():
    parser = argparse.ArgumentParser(
        description='Multi-Device Serial Monitor for SwarmLoc',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Monitor all connected devices
  python3 monitor_swarm.py

  # Monitor specific devices
  python3 monitor_swarm.py --devices /dev/ttyACM0 /dev/ttyACM1

  # Show ranging matrix only
  python3 monitor_swarm.py --matrix

  # Specify baud rate
  python3 monitor_swarm.py --baud 9600
        """
    )

    parser.add_argument('--devices', nargs='+',
                        help='List of device paths (e.g., /dev/ttyACM0)')
    parser.add_argument('--baud', type=int, default=115200,
                        help='Baud rate (default: 115200)')
    parser.add_argument('--matrix', action='store_true',
                        help='Show ranging matrix only')
    parser.add_argument('--no-color', action='store_true',
                        help='Disable colored output')

    args = parser.parse_args()

    # Detect devices
    if args.devices:
        devices = args.devices
    else:
        print("Detecting Arduino devices...")
        devices = detect_arduino_devices()

    if not devices:
        print("ERROR: No Arduino devices found!")
        print("Please connect Arduino boards and try again.")
        sys.exit(1)

    print(f"Found {len(devices)} device(s):")
    for device in devices:
        print(f"  - {device}")
    print()

    # Start monitoring threads
    threads = []
    for device in devices:
        thread = threading.Thread(target=monitor_device, args=(device,), daemon=True)
        thread.start()
        threads.append(thread)
        time.sleep(0.5)  # Stagger starts

    # Start display thread
    if args.matrix:
        # Matrix-only mode
        try:
            display_ranging_matrix()
        except KeyboardInterrupt:
            pass
    else:
        # Combined mode
        display_thread = threading.Thread(target=display_output, daemon=True)
        display_thread.start()

        try:
            # Keep main thread alive
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            print("\nShutting down...")

    # Cleanup
    print("Closing log files...")
    for log_file in log_files.values():
        log_file.close()

    print("Done.")


if __name__ == '__main__':
    main()
