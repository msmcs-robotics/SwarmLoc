#!/bin/bash
# Quick live ranging test with diagnostic firmware

set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  Live DW1000 Ranging Test${NC}"
echo -e "${GREEN}========================================${NC}"
echo

# Create temp project for ANCHOR
echo -e "${YELLOW}=== Compiling ANCHOR ===${NC}"
TEMP_ANCHOR="/tmp/ranging_anchor_$$"
mkdir -p "$TEMP_ANCHOR/src"
mkdir -p "$TEMP_ANCHOR/lib"

# Copy diagnostic code and set IS_ANCHOR true
sed 's/#define IS_ANCHOR false/#define IS_ANCHOR true/' "$SCRIPT_DIR/test_diagnostic.ino" > "$TEMP_ANCHOR/src/main.cpp"
ln -s "$PROJECT_ROOT/lib/DW1000" "$TEMP_ANCHOR/lib/DW1000"

cat > "$TEMP_ANCHOR/platformio.ini" <<EOF
[env:uno]
platform = atmelavr
board = uno
framework = arduino
upload_port = /dev/ttyACM0
monitor_port = /dev/ttyACM0
monitor_speed = 115200
lib_deps = SPI
EOF

cd "$TEMP_ANCHOR"
pio run -s && pio run -t upload -s

echo
echo -e "${YELLOW}=== Compiling TAG ===${NC}"
TEMP_TAG="/tmp/ranging_tag_$$"
mkdir -p "$TEMP_TAG/src"
mkdir -p "$TEMP_TAG/lib"

# Copy diagnostic code (IS_ANCHOR already false)
cp "$SCRIPT_DIR/test_diagnostic.ino" "$TEMP_TAG/src/main.cpp"
ln -s "$PROJECT_ROOT/lib/DW1000" "$TEMP_TAG/lib/DW1000"

cat > "$TEMP_TAG/platformio.ini" <<EOF
[env:uno]
platform = atmelavr
board = uno
framework = arduino
upload_port = /dev/ttyACM1
monitor_port = /dev/ttyACM1
monitor_speed = 115200
lib_deps = SPI
EOF

cd "$TEMP_TAG"
pio run -s && pio run -t upload -s

echo
echo -e "${GREEN}========================================${NC}"
echo "Both devices uploaded!"
echo -e "${GREEN}========================================${NC}"
echo
echo "Now monitoring both for 60 seconds..."
echo

# Monitor both devices
python3 -c "
import serial
import time
import threading

def monitor_port(port, name):
    try:
        ser = serial.Serial(port, 115200, timeout=1)
        time.sleep(2)
        print(f'\\n=== {name} OUTPUT ===')
        start = time.time()
        while (time.time() - start) < 60:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    print(f'[{name}] {line}')
        ser.close()
    except Exception as e:
        print(f'{name} Error: {e}')

t1 = threading.Thread(target=monitor_port, args=('/dev/ttyACM0', 'ANCHOR'))
t2 = threading.Thread(target=monitor_port, args=('/dev/ttyACM1', 'TAG'))

t1.start()
t2.start()

t1.join()
t2.join()

print('\\n=== Monitoring Complete ===')
"

echo
echo -e "${YELLOW}If you saw ranging measurements above, SUCCESS!${NC}"
echo -e "${YELLOW}If only heartbeats, the devices didn't find each other.${NC}"
