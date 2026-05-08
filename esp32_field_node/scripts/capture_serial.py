#!/usr/bin/env python3
"""Capture serial output from an ESP32 with optional reset on connect.

Usage:
    scripts/capture_serial.py [PORT] [DURATION_SEC] [--no-reset] [--baud N]

Defaults: PORT=/dev/ttyUSB0, DURATION_SEC=10, baud=115200.

Writes captured output to stdout. Tee externally if you want a file.
"""

import sys
import time

try:
    import serial
except ImportError:
    print("ERROR: pyserial not installed. Run: pip install pyserial",
          file=sys.stderr)
    sys.exit(2)


def main() -> int:
    port = "/dev/ttyUSB0"
    duration = 10.0
    baud = 115200
    do_reset = True

    args = sys.argv[1:]
    positional = []
    i = 0
    while i < len(args):
        a = args[i]
        if a == "--no-reset":
            do_reset = False
        elif a == "--baud":
            baud = int(args[i + 1])
            i += 1
        else:
            positional.append(a)
        i += 1

    if positional:
        port = positional[0]
    if len(positional) > 1:
        duration = float(positional[1])

    try:
        s = serial.Serial(port, baud, timeout=0.3)
    except serial.SerialException as e:
        print(f"ERROR: cannot open {port}: {e}", file=sys.stderr)
        return 1

    if do_reset:
        # ESP32 DevKit (CP2102) reset:
        #   DTR -> IO0 (inverting transistor)
        #   RTS -> EN  (inverting transistor)
        # To boot normally: IO0 must be HIGH during reset rising edge.
        # pyserial setDTR(False) -> DTR line idle -> IO0 HIGH (good).
        # pyserial setRTS(True)  -> RTS asserted -> EN LOW (in reset).
        s.dtr = False
        s.rts = True
        time.sleep(0.1)
        s.rts = False

    deadline = time.time() + duration
    while time.time() < deadline:
        line = s.readline()
        if line:
            sys.stdout.write(line.decode("utf-8", "replace"))
            sys.stdout.flush()
    s.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
