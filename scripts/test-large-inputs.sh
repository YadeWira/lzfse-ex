#!/bin/sh
# Regression test for large inputs (#64).
#
# Compresses, decompresses and byte-compares a file larger than 2 GiB (3 GiB
# by default). Before the 1.0.0 fix, encoding inputs between 2 and 4 GiB
# overflowed the encoder's 32-bit offsets and silently stopped finding matches
# after the 2 GiB mark.
#
# Usage:
#   scripts/test-large-inputs.sh [cli]
#
# Environment:
#   SIZE_MB   size of the test input in MiB (default: 3072)
#   WORK      scratch directory (default: build/large-test)
#   CLI       lzfse command line tool (default: build/bin/lzfse,
#             then build/matrix/linux-x64/lzfse)
#
# Requires python3 (data generation), about 3x SIZE_MB of free disk space and
# about 2x SIZE_MB of RAM (plus the decoded output).

set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SIZE_MB=${SIZE_MB:-3072}
WORK=${WORK:-$ROOT/build/large-test}

if [ $# -ge 1 ]; then
  CLI=$1
elif [ -x "$ROOT/build/bin/lzfse" ]; then
  CLI="$ROOT/build/bin/lzfse"
elif [ -x "$ROOT/build/matrix/linux-x64/lzfse" ]; then
  CLI="$ROOT/build/matrix/linux-x64/lzfse"
else
  echo "lzfse CLI not found; build first or pass it as the first argument" >&2
  exit 1
fi

mkdir -p "$WORK"

echo "generating $SIZE_MB MiB in $WORK ..."
python3 - "$WORK" "$SIZE_MB" <<'EOF'
import os
import random
import sys

work = sys.argv[1]
size = int(sys.argv[2]) * 1024 * 1024

# A 200,000-byte random pattern, repeated. Matches exist at a 200 KB distance
# (below the encoder's maximum distance), so the encoder must find matches
# throughout the whole input, including past the 2 GiB mark.
pattern_path = os.path.join(work, "pattern.bin")
if not os.path.exists(pattern_path):
    rnd = random.Random(1234)
    pattern = bytes(rnd.getrandbits(8) for _ in range(200000))
    with open(pattern_path, "wb") as f:
        f.write(pattern)
else:
    with open(pattern_path, "rb") as f:
        pattern = f.read()

with open(os.path.join(work, "input.bin"), "wb") as f:
    written = 0
    while written < size:
        chunk = pattern[: min(len(pattern), size - written)]
        f.write(chunk)
        written += len(chunk)
EOF

echo "compressing ..."
start=$(date +%s)
"$CLI" -encode -i "$WORK/input.bin" -o "$WORK/input.lzfse"
mid=$(date +%s)

echo "decoding ..."
"$CLI" -decode -i "$WORK/input.lzfse" -o "$WORK/input.out"
end=$(date +%s)

if cmp "$WORK/input.bin" "$WORK/input.out"; then
  echo "OK: roundtrip of $(wc -c < "$WORK/input.bin") bytes, compressed to $(wc -c < "$WORK/input.lzfse") bytes"
  echo "    encode $((mid - start))s, decode $((end - mid))s"
else
  echo "FAIL: decoded output differs from the input" >&2
  exit 1
fi

echo "removing test data"
rm -f "$WORK/input.bin" "$WORK/input.out" "$WORK/input.lzfse"
