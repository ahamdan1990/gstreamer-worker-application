#!/usr/bin/env python3
"""
Quick tool to visualize face detection coordinates
"""

# From your test output
bbox_x = 0.435
bbox_y = 0.216
bbox_w = 0.173
bbox_h = 0.370

# Your video resolution (from config: max 1280x720)
frame_width = 1280
frame_height = 720

# Convert to pixel coordinates
pixel_x = int(bbox_x * frame_width)
pixel_y = int(bbox_y * frame_height)
pixel_w = int(bbox_w * frame_width)
pixel_h = int(bbox_h * frame_height)

print("="*60)
print("FACE DETECTION VERIFICATION")
print("="*60)
print(f"\nFrame Size: {frame_width}x{frame_height}")
print(f"\nNormalized BBox: [{bbox_x:.3f}, {bbox_y:.3f}] {bbox_w:.3f}x{bbox_h:.3f}")
print(f"\nPixel Coordinates:")
print(f"  Top-Left Corner: ({pixel_x}, {pixel_y})")
print(f"  Box Size: {pixel_w}x{pixel_h} pixels")
print(f"  Bottom-Right: ({pixel_x + pixel_w}, {pixel_y + pixel_h})")
print(f"\nPosition Description:")
print(f"  - Horizontally: {bbox_x*100:.1f}% from left (center at {(bbox_x + bbox_w/2)*100:.1f}%)")
print(f"  - Vertically: {bbox_y*100:.1f}% from top (center at {(bbox_y + bbox_h/2)*100:.1f}%)")
print(f"  - Face takes up {(bbox_w * bbox_h * 100):.1f}% of frame area")
print("="*60)
