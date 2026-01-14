#!/bin/bash
# 将 BGRA 裸帧数据转换为视频
# 用法: ./convert_frames.sh <input_dir> [width] [height] [fps]

set -e

INPUT_DIR="${1:-.}"
WIDTH="${2:-1920}"
HEIGHT="${3:-1080}"
FPS="${4:-30}"

if [ ! -d "$INPUT_DIR" ]; then
    echo "Error: Input directory '$INPUT_DIR' not found"
    exit 1
fi

FIRST_FRAME=$(ls "$INPUT_DIR"/frame-*.bgra 2>/dev/null | head -1)
if [ -z "$FIRST_FRAME" ]; then
    echo "Error: No .bgra frames found in '$INPUT_DIR'"
    exit 1
fi

echo "Converting ${WIDTH}x${HEIGHT} @ ${FPS}fps..."

cat "$INPUT_DIR"/frame-*.bgra | ffmpeg -y -f rawvideo -pixel_format bgra \
    -video_size "${WIDTH}x${HEIGHT}" -framerate "$FPS" -i - \
    -c:v libx264 -pix_fmt yuv420p "$INPUT_DIR/output.mp4"

echo "Video saved to: $INPUT_DIR/output.mp4"
