#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$ROOT_DIR/builddir"
OUT_DIR="/tmp/cgif_bench_bin"
RESULTS_FILE="/tmp/cgif_bench_results.txt"

mkdir -p "$OUT_DIR"
> "$RESULTS_FILE"

# Build cgif in release mode
meson setup "$BUILD_DIR" "$ROOT_DIR" --buildtype=release --wipe >/dev/null 2>&1
meson compile -C "$BUILD_DIR" >/dev/null 2>&1

# Build benchmarks
echo "Building benchmarks..."
cc -O3 -o "$OUT_DIR/cgif_bench" "$SCRIPT_DIR/cgif_bench.c" \
  -I"$ROOT_DIR/inc" -L"$BUILD_DIR" -lcgif -Wl,-rpath,"$BUILD_DIR"

cc -O3 -o "$OUT_DIR/giflib_bench" "$SCRIPT_DIR/giflib_bench.c" \
  -I/opt/homebrew/include -L/opt/homebrew/lib -lgif 2>/dev/null

# giflib-turbo (optional)
GIFLIB_TURBO=""
if [ -d /tmp/giflib-turbo ]; then
  cc -O3 -o "$OUT_DIR/giflib_turbo_bench" "$SCRIPT_DIR/giflib_bench.c" \
    -I/tmp/giflib-turbo /tmp/giflib-turbo/gif_lib.c 2>/dev/null
  GIFLIB_TURBO=1
fi

# ffmpeg (optional)
FFMPEG=""
if pkg-config --exists libavcodec libavutil 2>/dev/null; then
  cc -O3 -o "$OUT_DIR/ffmpeg_bench" "$SCRIPT_DIR/ffmpeg_bench.c" \
    $(pkg-config --cflags --libs libavcodec libavutil) 2>/dev/null
  FFMPEG=1
fi

PATTERNS="solid gradient stripe checker dither noise fewnoise"
RUNS="${1:-5}"

# Build list of encoders
ENCODERS="cgif giflib"
[ -n "$GIFLIB_TURBO" ] && ENCODERS="$ENCODERS giflib-turbo"
[ -n "$FFMPEG" ] && ENCODERS="$ENCODERS ffmpeg"

echo "Running benchmarks (${RUNS} runs per test)..."
echo ""

for pattern in $PATTERNS; do
  ARGS="-n cgif '$OUT_DIR/cgif_bench $pattern'"
  ARGS="$ARGS -n giflib '$OUT_DIR/giflib_bench $pattern'"
  if [ -n "$GIFLIB_TURBO" ]; then
    ARGS="$ARGS -n giflib-turbo '$OUT_DIR/giflib_turbo_bench $pattern 2>/dev/null'"
  fi
  if [ -n "$FFMPEG" ]; then
    ARGS="$ARGS -n ffmpeg '$OUT_DIR/ffmpeg_bench $pattern'"
  fi

  echo "=== $pattern ==="
  EXPORT_JSON="$OUT_DIR/${pattern}.json"
  eval DYLD_LIBRARY_PATH="$BUILD_DIR" hyperfine --warmup 2 --runs "$RUNS" \
    --export-json "$EXPORT_JSON" $ARGS
  echo ""

  # Extract mean times from JSON
  for encoder in $ENCODERS; do
    mean=$(python3 -c "
import sys, json
data = json.load(open('$EXPORT_JSON'))
for r in data['results']:
    if r['command'] == '${encoder}':
        print(f\"{r['mean']*1000:.1f}\")
        break
" 2>/dev/null || echo "n/a")
    echo "$pattern $encoder $mean" >> "$RESULTS_FILE"
  done
done

# Print summary table
echo ""
echo "=============================="
echo "  Summary (mean ms, 8192x8192)"
echo "=============================="
echo ""

# Header
HEADER=$(printf "%-12s" "pattern")
for encoder in $ENCODERS; do
  HEADER="$HEADER $(printf "%14s" "$encoder")"
done
HEADER="$HEADER $(printf "%12s" "vs giflib")"
[ -n "$FFMPEG" ] && HEADER="$HEADER $(printf "%12s" "vs ffmpeg")"
echo "$HEADER"
echo "$HEADER" | sed 's/./-/g'

for pattern in $PATTERNS; do
  ROW=$(printf "%-12s" "$pattern")
  CGIF_MS=""
  GIFLIB_MS=""
  FFMPEG_MS=""
  for encoder in $ENCODERS; do
    ms=$(grep "^$pattern $encoder " "$RESULTS_FILE" | awk '{print $3}')
    ROW="$ROW $(printf "%11s ms" "$ms")"
    case $encoder in
      cgif) CGIF_MS=$ms ;;
      giflib) GIFLIB_MS=$ms ;;
      ffmpeg) FFMPEG_MS=$ms ;;
    esac
  done

  # Compute speedup vs giflib
  if [ -n "$CGIF_MS" ] && [ -n "$GIFLIB_MS" ]; then
    speedup=$(python3 -c "print(f'{${GIFLIB_MS}/${CGIF_MS}:.2f}x')")
    ROW="$ROW $(printf "%12s" "$speedup")"
  fi

  # Compute speedup vs ffmpeg
  if [ -n "$FFMPEG" ] && [ -n "$CGIF_MS" ] && [ -n "$FFMPEG_MS" ]; then
    speedup=$(python3 -c "print(f'{${FFMPEG_MS}/${CGIF_MS}:.2f}x')")
    ROW="$ROW $(printf "%12s" "$speedup")"
  fi

  echo "$ROW"
done

echo ""
