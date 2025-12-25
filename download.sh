#!/bin/bash

# Download LittleFS filesystem from ESP32 to data_download folder using PlatformIO
# Usage: ./download.sh

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Partition info from custom_app.csv
SPIFFS_OFFSET=0x210000
SPIFFS_SIZE_DEC=$((0x1E0000))

# Output directory
OUTPUT_DIR="data_download"

echo -e "${GREEN}Downloading LittleFS filesystem from ESP32 using PlatformIO...${NC}"
echo ""

# Get serial port from PlatformIO
# pio device list format shows ports on separate lines, prioritize USB serial ports
PORT=$(pio device list 2>/dev/null | grep -E '/dev/(cu|tty)' | grep -v 'debug-console\|Bluetooth' | grep -E 'usbserial|USB|ttyUSB|ttyACM' | head -1 | awk '{print $1}' || echo '')

# If no USB port found, try any serial port (excluding debug/Bluetooth)
if [ -z "$PORT" ]; then
    PORT=$(pio device list 2>/dev/null | grep -E '/dev/(cu|tty)' | grep -v 'debug-console\|Bluetooth' | head -1 | awk '{print $1}' || echo '')
fi

if [ -z "$PORT" ]; then
    echo -e "${RED}Error: No serial port found. Please connect your ESP32.${NC}"
    echo ""
    echo "Available ports:"
    pio device list
    exit 1
fi

echo "Using port: $PORT"
echo "Offset: $SPIFFS_OFFSET"
echo "Size: $(($SPIFFS_SIZE_DEC / 1024))KB"
echo ""

# Find PlatformIO's esptool
PLATFORMIO_ESPTOOL=$(find ~/.platformio -name "esptool.py" -path "*/tool-esptoolpy/*" 2>/dev/null | head -1)
if [ -z "$PLATFORMIO_ESPTOOL" ]; then
    PLATFORMIO_ESPTOOL=$(find ~/.platformio -name "esptool.py" 2>/dev/null | head -1)
fi

if [ -z "$PLATFORMIO_ESPTOOL" ]; then
    echo -e "${RED}Error: esptool.py not found in PlatformIO${NC}"
    exit 1
fi

# Create output directory
rm -rf "$OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR"

FS_IMAGE="$OUTPUT_DIR/littlefs.bin"

# Download the filesystem partition using PlatformIO's esptool
# Use lower baud rate for reliability (115200 is more stable than 921600)
echo -e "${YELLOW}Reading filesystem from flash...${NC}"
python3 "$PLATFORMIO_ESPTOOL" --chip esp32 --port "$PORT" --baud 115200 read_flash $SPIFFS_OFFSET $SPIFFS_SIZE_DEC "$FS_IMAGE"

if [ ! -f "$FS_IMAGE" ] || [ ! -s "$FS_IMAGE" ]; then
    echo -e "${RED}Error: Failed to download filesystem${NC}"
    exit 1
fi

echo -e "${GREEN}Filesystem downloaded successfully!${NC}"
echo ""

# Extract files using Python littlefs library
if command -v python3 &> /dev/null; then
    echo -e "${YELLOW}Extracting files...${NC}"
    if python3 -c "import littlefs" 2>/dev/null; then
        python3 << EOF
import littlefs
import os

fs_image = "$FS_IMAGE"
output_dir = "$OUTPUT_DIR"

try:
    lfs = littlefs.LittleFS()
    with open(fs_image, 'rb') as f:
        lfs.read(f.read())
    
    extracted_count = 0
    for root, dirs, files in lfs.walk('/'):
        for file in files:
            full_path = os.path.join(root, file).replace('\\', '/')
            try:
                data = lfs.read_file(full_path)
                local_path = os.path.join(output_dir, full_path.lstrip('/'))
                os.makedirs(os.path.dirname(local_path), exist_ok=True)
                with open(local_path, 'wb') as out:
                    out.write(data)
                print(f"Extracted: {full_path}")
                extracted_count += 1
            except Exception as e:
                print(f"Error extracting {full_path}: {e}")
    
    print(f"\nExtracted {extracted_count} files successfully!")
except Exception as e:
    print(f"Error reading filesystem: {e}")
    print("Raw filesystem image saved to: $FS_IMAGE")
EOF
    else
        echo -e "${YELLOW}littlefs-python not installed. Raw filesystem image saved to: $FS_IMAGE${NC}"
        echo ""
        echo "To extract files, install: pip3 install littlefs-python"
    fi
fi

echo ""
echo -e "${GREEN}Done! Files saved to: $OUTPUT_DIR/${NC}"
