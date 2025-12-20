

"""
Convert images to loading screen pixel data format.

Usage:
    python image_to_loading_screen.py input.png output.txt [--width WIDTH] [--height HEIGHT] [--crop]

Options:
    --width WIDTH     Target width (default: 296)
    --height HEIGHT   Target height (default: 128)
    --crop            Crop instead of scale (default: scale to cover, then crop)
    --no-crop         Don't crop, scale to fit (maintains aspect ratio, may leave empty space)
"""

import argparse
import sys
from PIL import Image

def scale_to_cover(img, target_width, target_height):
    """Scale image to cover target dimensions while maintaining aspect ratio."""
    img_width, img_height = img.size
    target_aspect = target_width / target_height
    img_aspect = img_width / img_height
    
    if img_aspect > target_aspect:
        

        new_height = target_height
        new_width = int(img_width * (target_height / img_height))
    else:
        

        new_width = target_width
        new_height = int(img_height * (target_width / img_width))
    
    return img.resize((new_width, new_height), Image.Resampling.LANCZOS)

def crop_center(img, target_width, target_height):
    """Crop image to center, maintaining target dimensions."""
    img_width, img_height = img.size
    left = (img_width - target_width) // 2
    top = (img_height - target_height) // 2
    right = left + target_width
    bottom = top + target_height
    return img.crop((left, top, right, bottom))

def convert_to_monochrome(img, threshold=128):
    """Convert image to monochrome (black/white) using threshold."""
    

    if img.mode != 'L':
        img = img.convert('L')
    
    

    img = img.point(lambda x: 1 if x >= threshold else 0, mode='1')
    return img

def image_to_text_format(img, target_width, target_height, crop=True, threshold=128):
    """
    Convert image to text format for loading screen.
    
    Format:
    - First line: WIDTH HEIGHT
    - Subsequent lines: pixel data, one row per line
    - Each pixel: '1' for white, '0' for black/transparent
    """
    

    img = scale_to_cover(img, target_width, target_height)
    
    

    if crop:
        img = crop_center(img, target_width, target_height)
    else:
        

        img.thumbnail((target_width, target_height), Image.Resampling.LANCZOS)
        

        new_img = Image.new('1', (target_width, target_height), 0)
        paste_x = (target_width - img.width) // 2
        paste_y = (target_height - img.height) // 2
        new_img.paste(img, (paste_x, paste_y))
        img = new_img
    
    

    if img.size != (target_width, target_height):
        img = img.resize((target_width, target_height), Image.Resampling.NEAREST)
    
    

    img = convert_to_monochrome(img, threshold)
    
    

    pixels = img.load()
    lines = []
    
    

    lines.append(f"{target_width} {target_height}")
    
    

    for y in range(target_height):
        row = ""
        for x in range(target_width):
            pixel = pixels[x, y]
            row += "1" if pixel else "0"
        lines.append(row)
    
    return "\n".join(lines)

def main():
    parser = argparse.ArgumentParser(
        description="Convert images to loading screen pixel data format",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument("input", help="Input image file")
    parser.add_argument("output", help="Output text file")
    parser.add_argument("--width", type=int, default=296, help="Target width (default: 296)")
    parser.add_argument("--height", type=int, default=128, help="Target height (default: 128)")
    parser.add_argument("--crop", action="store_true", default=True, help="Crop to exact size after scaling (default)")
    parser.add_argument("--no-crop", action="store_false", dest="crop", help="Scale to fit without cropping")
    parser.add_argument("--threshold", type=int, default=128, help="Monochrome threshold 0-255 (default: 128)")
    
    args = parser.parse_args()
    
    

    if args.width <= 0 or args.height <= 0:
        print("Error: Width and height must be positive", file=sys.stderr)
        sys.exit(1)
    
    

    try:
        img = Image.open(args.input)
    except Exception as e:
        print(f"Error loading image: {e}", file=sys.stderr)
        sys.exit(1)
    
    

    if img.width < args.width or img.height < args.height:
        print(f"Warning: Image ({img.width}x{img.height}) is smaller than target ({args.width}x{args.height})", file=sys.stderr)
        print("Image will be upscaled, which may reduce quality", file=sys.stderr)
    
    

    try:
        result = image_to_text_format(img, args.width, args.height, args.crop, args.threshold)
        
        

        with open(args.output, 'w') as f:
            f.write(result)
        
        print(f"Successfully converted {args.input} to {args.output}")
        print(f"Output size: {args.width}x{args.height} pixels")
        print(f"Mode: {'Cropped' if args.crop else 'Scaled to fit'}")
        
    except Exception as e:
        print(f"Error converting image: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
