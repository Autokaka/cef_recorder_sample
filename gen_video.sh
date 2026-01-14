#!/bin/bash
# 将 BGRA 裸帧数据转换为视频（自动填补缺失帧）
# 用法: ./gen_video.sh <input_dir> [width] [height] [fps]

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

echo "Filling missing frames..."

# 获取最大帧号
MAX_FRAME=$(ls "$INPUT_DIR"/frame-*.bgra | sed 's/.*frame-\([0-9]*\)\.bgra/\1/' | sort -n | tail -1)
MAX_FRAME=$((10#$MAX_FRAME))  # 移除前导零

LAST_FRAME=""
FILLED=0

for i in $(seq 0 $MAX_FRAME); do
    PADDED=$(printf "%06d" $i)
    FRAME="$INPUT_DIR/frame-$PADDED.bgra"
    
    if [ -f "$FRAME" ]; then
        LAST_FRAME="$FRAME"
    elif [ -n "$LAST_FRAME" ]; then
        # 帧缺失，创建符号链接复用上一帧
        ln -s "$(basename "$LAST_FRAME")" "$FRAME"
        ((FILLED++))
    fi
done

echo "Filled $FILLED missing frames (total: $((MAX_FRAME + 1)) frames)"
echo "Converting ${WIDTH}x${HEIGHT} @ ${FPS}fps..."

cat "$INPUT_DIR"/frame-*.bgra | ffmpeg -y -f rawvideo -pixel_format bgra \
    -video_size "${WIDTH}x${HEIGHT}" -framerate "$FPS" -i - \
    -c:v libx264 -pix_fmt yuv420p "$INPUT_DIR/output.mp4"

echo "Video saved to: $INPUT_DIR/output.mp4"
