#!/usr/bin/env python3
"""
Test SCRFD model to verify it's working correctly
This will help diagnose if the issue is with the model or the C++ implementation
"""

import cv2
import numpy as np
import onnxruntime as ort
import sys

def preprocess_image(img, input_size=640):
    """Preprocess image for SCRFD model"""
    # Resize to input size
    h, w = img.shape[:2]
    scale = input_size / max(h, w)
    new_h = int(h * scale)
    new_w = int(w * scale)

    resized = cv2.resize(img, (new_w, new_h))

    # Pad to square
    padded = np.zeros((input_size, input_size, 3), dtype=np.uint8)
    padded[:new_h, :new_w] = resized

    # Convert BGR to RGB
    rgb = cv2.cvtColor(padded, cv2.COLOR_BGR2RGB)

    # Normalize to [-1, 1]
    normalized = (rgb.astype(np.float32) / 128.0) - 1.0

    # CHW format
    transposed = normalized.transpose(2, 0, 1)

    # Add batch dimension
    batched = np.expand_dims(transposed, axis=0)

    return batched, scale

def main():
    # Model path
    model_path = "models/scrfd/scrfd_10g_bnkps.onnx"

    print(f"Loading model: {model_path}")

    # Create ONNX Runtime session
    session = ort.InferenceSession(model_path, providers=['CPUExecutionProvider'])

    # Get model info
    print(f"\nModel inputs: {[inp.name for inp in session.get_inputs()]}")
    print(f"Model outputs: {[out.name for out in session.get_outputs()]}")

    # Capture frame from RTSP stream
    rtsp_url = "rtsp://service:WSS4Sec$$@192.168.0.219"
    print(f"\nCapturing frame from: rtsp://service:***@192.168.0.219")

    cap = cv2.VideoCapture(rtsp_url)
    if not cap.isOpened():
        print("ERROR: Could not open RTSP stream")
        return 1

    # Read frame
    ret, frame = cap.read()
    if not ret:
        print("ERROR: Could not read frame")
        return 1

    print(f"Frame shape: {frame.shape}")

    # Save original frame
    cv2.imwrite("test_frame_original.jpg", frame)
    print("Saved: test_frame_original.jpg")

    # Preprocess
    input_data, scale = preprocess_image(frame, 640)
    print(f"Input shape: {input_data.shape}")
    print(f"Input min/max: {input_data.min():.3f} / {input_data.max():.3f}")

    # Run inference
    print("\nRunning inference...")
    input_name = session.get_inputs()[0].name
    outputs = session.run(None, {input_name: input_data})

    print(f"\nOutput shapes:")
    for i, out in enumerate(outputs):
        print(f"  Output {i}: {out.shape}, min/max: {out.min():.3f} / {out.max():.3f}")

    # Analyze score outputs (first 3 outputs)
    print(f"\nScore analysis:")
    for i in range(3):
        scores = outputs[i]
        max_score = scores.max()
        num_above_03 = (scores > 0.3).sum()
        num_above_05 = (scores > 0.5).sum()
        print(f"  Scale {i}: max={max_score:.3f}, >0.3: {num_above_03}, >0.5: {num_above_05}")

    # Simple visualization (just mark high-confidence scores)
    vis_frame = frame.copy()
    h, w = frame.shape[:2]

    strides = [8, 16, 32]
    for scale_idx in range(3):
        stride = strides[scale_idx]
        scores = outputs[scale_idx].flatten()
        bboxes = outputs[3 + scale_idx]

        fmap_h = 640 // stride
        fmap_w = 640 // stride

        for i in range(min(len(scores), fmap_h * fmap_w * 2)):
            score = scores[i]
            if score > 0.3:  # Confidence threshold
                # Anchor position
                anchor_idx = i // 2
                aspect_idx = i % 2

                y_pos = anchor_idx // fmap_w
                x_pos = anchor_idx % fmap_w

                cx = (x_pos + 0.5) * stride
                cy = (y_pos + 0.5) * stride

                # Scale back to original image
                cx = int(cx / scale)
                cy = int(cy / scale)

                # Draw marker
                color = (0, 255, 0) if score > 0.5 else (0, 255, 255)
                cv2.circle(vis_frame, (cx, cy), 5, color, -1)
                cv2.putText(vis_frame, f"{score:.2f}", (cx+10, cy),
                           cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 1)

    cv2.imwrite("test_frame_detections.jpg", vis_frame)
    print("\nSaved: test_frame_detections.jpg")

    cap.release()
    return 0

if __name__ == "__main__":
    sys.exit(main())
