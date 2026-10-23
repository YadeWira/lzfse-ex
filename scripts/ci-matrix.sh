#!/bin/sh
# Build and test LZFSE for the supported targets, from a Linux host.
#
#   Linux x64, Linux x86 (gcc -m32), Linux arm64 and armhf (cross-compiled,
#   run under qemu-user), Windows x64 and x86 (MinGW-w64, run under Wine).
#
# Targets whose toolchain is not installed are skipped with a message. The
# script exits non-zero if any configured target fails to build or to pass
# its tests. Build trees go to build/matrix/, which is git-ignored.
#
# Usage:
#   scripts/ci-matrix.sh            # all available targets
#   scripts/ci-matrix.sh linux-arm64 windows-x64
#
# Environment:
#   CC, CXX   host compiler (default: cc)
#   JOBS      parallel build jobs (default: number of CPUs)

set -u

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD="$ROOT/build/matrix"
CC=${CC:-cc}
JOBS=${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}

REQUESTED="$*"
FAILED=""
PASSED=""
SKIPPED=""

have() { command -v "$1" >/dev/null 2>&1; }

if ! have cmake; then
  echo "cmake not found" >&2
  exit 1
fi

# Compile a trivial program to check that a compiler works at all.
works() {
  tmp=$(mktemp -d 2>/dev/null || echo "${TMPDIR:-/tmp}/lzfse-ci-$$")
  mkdir -p "$tmp"
  printf 'int main(void){return 0;}\n' > "$tmp/t.c"
  if "$@" -x c "$tmp/t.c" -o "$tmp/t" >/dev/null 2>&1; then
    rc=0
  else
    rc=1
  fi
  rm -rf "$tmp"
  return $rc
}

build_and_test() {
  name=$1
  shift
  echo
  echo "=== $name: $*"
  rm -rf "$BUILD/$name"
  if ! cmake -S "$ROOT" -B "$BUILD/$name" -DCMAKE_BUILD_TYPE=Release "$@"; then
    echo "!!! $name: configure failed"
    FAILED="$FAILED $name(configure)"
    return
  fi
  if ! cmake --build "$BUILD/$name" --parallel "$JOBS"; then
    echo "!!! $name: build failed"
    FAILED="$FAILED $name(build)"
    return
  fi
  if ! (cd "$BUILD/$name" && ctest --output-on-failure); then
    echo "!!! $name: tests failed"
    FAILED="$FAILED $name(test)"
    return
  fi
  echo "+++ $name: OK"
  PASSED="$PASSED $name"
}

want() {
  # With no arguments, run everything; otherwise run only what was asked for.
  [ -z "$REQUESTED" ] && return 0
  for name in $REQUESTED; do
    [ "$name" = "$TARGET" ] && return 0
  done
  return 1
}

run_target() {
  TARGET=$1
  shift
  if want; then
    "$@"
  fi
}

# --- Linux, native ---------------------------------------------------------
if works "$CC"; then
  run_target linux-x64 build_and_test linux-x64 -DCMAKE_C_COMPILER="$CC"
else
  SKIPPED="$SKIPPED linux-x64(no compiler)"
fi

# --- Linux, 32-bit x86 -----------------------------------------------------
if works "$CC" -m32; then
  run_target linux-x86 build_and_test linux-x86 \
    -DCMAKE_C_COMPILER="$CC" -DCMAKE_C_FLAGS=-m32 -DCMAKE_EXE_LINKER_FLAGS=-m32
else
  SKIPPED="$SKIPPED linux-x86(no 32-bit libc)"
fi

# --- Linux ARM64 (qemu-user) ----------------------------------------------
if have aarch64-linux-gnu-gcc && have qemu-aarch64; then
  run_target linux-arm64 build_and_test linux-arm64 \
    -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
    "-DCMAKE_CROSSCOMPILING_EMULATOR=qemu-aarch64;-L;/usr/aarch64-linux-gnu"
else
  SKIPPED="$SKIPPED linux-arm64(no aarch64 toolchain or qemu-user)"
fi

# --- Linux ARM hard-float (qemu-user) -------------------------------------
if have arm-linux-gnueabihf-gcc && have qemu-arm; then
  run_target linux-armhf build_and_test linux-armhf \
    -DCMAKE_C_COMPILER=arm-linux-gnueabihf-gcc \
    "-DCMAKE_CROSSCOMPILING_EMULATOR=qemu-arm;-L;/usr/arm-linux-gnueabihf"
else
  SKIPPED="$SKIPPED linux-armhf(no armhf toolchain or qemu-user)"
fi

# --- Windows x64 / x86 (MinGW-w64, tested under Wine) ----------------------
if have x86_64-w64-mingw32-gcc; then
  run_target windows-x64 build_and_test windows-x64 \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc
else
  SKIPPED="$SKIPPED windows-x64(no mingw-w64)"
fi

if have i686-w64-mingw32-gcc; then
  run_target windows-x86 build_and_test windows-x86 \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=i686-w64-mingw32-gcc
else
  SKIPPED="$SKIPPED windows-x86(no mingw-w64)"
fi

# --- AddressSanitizer + UndefinedBehaviorSanitizer -------------------------
if works "$CC" -fsanitize=address,undefined; then
  run_target sanitizers build_and_test sanitizers \
    -DCMAKE_C_COMPILER="$CC" -DENABLE_SANITIZER=address,undefined
else
  SKIPPED="$SKIPPED sanitizers(no sanitizer support)"
fi

echo
echo "=== Summary"
[ -n "$PASSED" ] && echo "passed:$PASSED"
[ -n "$SKIPPED" ] && echo "skipped:$SKIPPED"
if [ -n "$FAILED" ]; then
  echo "FAILED:$FAILED"
  exit 1
fi
echo "all configured targets passed"
