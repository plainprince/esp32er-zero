






set -e



SERIAL_PORT="/dev/cu.usbserial-0001"
if lsof "$SERIAL_PORT" &>/dev/null; then
    echo "=== Killing processes using serial port ==="
    lsof -t "$SERIAL_PORT" | xargs kill 2>/dev/null || true
    sleep 1
fi

echo "=== Building and uploading firmware ==="
pio run --target upload

echo ""
echo "=== Building and uploading filesystem ==="
pio run --target uploadfs

echo ""
echo "=== Starting serial monitor (Ctrl+C to exit) ==="
pio device monitor

