# Tools

## image_to_loading_screen.py

Converts images to pixel data format for loading screens.

### Requirements

```bash
pip install Pillow
```

### Usage

```bash
python image_to_loading_screen.py input.png output.txt [options]
```

### Options

- `--width WIDTH`: Target width in pixels (default: 296)
- `--height HEIGHT`: Target height in pixels (default: 128)
- `--crop`: Crop to exact size after scaling (default behavior)
- `--no-crop`: Scale to fit without cropping (maintains aspect ratio, may leave empty space)
- `--threshold THRESHOLD`: Monochrome conversion threshold 0-255 (default: 128)

### Default Behavior

By default, the script:
1. Scales the image to cover the target dimensions (296x128) while maintaining aspect ratio
2. Crops the excess from the center to exactly match the target size
3. Converts to monochrome (black/white) using a threshold
4. Outputs a text file with pixel data

### Output Format

The output text file format:
- First line: `WIDTH HEIGHT` (e.g., `296 128`)
- Subsequent lines: One row per line, each character is a pixel
  - `1` = white pixel
  - `0` = black/transparent pixel

### Example

```bash
# Convert a 1920x1080 image to 296x128 loading screen
python image_to_loading_screen.py splash.png loading_screen.txt

# Convert with custom dimensions
python image_to_loading_screen.py splash.png loading_screen.txt --width 128 --height 64

# Scale to fit without cropping
python image_to_loading_screen.py splash.png loading_screen.txt --no-crop
```
