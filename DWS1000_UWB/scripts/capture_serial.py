#!/usr/bin/env python3
"""
Serial capture script with proper port management.
Usage: python3 capture_serial.py [port] [output_file] [max_lines] [timeout_sec]
"""

import sys
import time
import serial

def main():
    port = sys.argv[1] if len(sys.argv) > 1 else '/dev/ttyACM0'
    output = sys.argv[2] if len(sys.argv) > 2 else '/tmp/serial_out.txt'
    max_lines = int(sys.argv[3]) if len(sys.argv) > 3 else 150
    timeout_sec = int(sys.argv[4]) if len(sys.argv) > 4 else 45

    print(f"Capturing from {port} to {output} (max {max_lines} lines, {timeout_sec}s timeout)")

    try:
        ser = serial.Serial(port, 115200, timeout=1)

        # Reset device via DTR
        ser.dtr = False
        time.sleep(0.1)
        ser.dtr = True
        time.sleep(1.5)  # Wait for boot

        lines = []
        start = time.time()
        empty_count = 0

        while len(lines) < max_lines and (time.time() - start) < timeout_sec:
            try:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    lines.append(line)
                    print(line)
                    empty_count = 0
                else:
                    empty_count += 1
                    if empty_count > 15:  # 15 seconds of no data = done
                        break
            except Exception as e:
                print(f"[Read error: {e}]")
                break

        ser.close()

        # Write to file
        with open(output, 'w') as f:
            f.write('\n'.join(lines))

        print(f"\n[Captured {len(lines)} lines to {output}]")

    except Exception as e:
        print(f"[Error: {e}]")
        sys.exit(1)

if __name__ == '__main__':
    main()
