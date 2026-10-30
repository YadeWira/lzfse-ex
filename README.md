LZFSE (lzfse-ex)
================

`lzfse-ex` is a fork of the Apple LZFSE reference implementation, focused on
portable, robust builds. See [CHANGELOG.md](CHANGELOG.md) for the changes in
this fork.

| Component | Status |
|---|---|
| CI | [![CI](https://github.com/YadeWira/lzfse-ex/actions/workflows/ci.yml/badge.svg)](https://github.com/YadeWira/lzfse-ex/actions/workflows/ci.yml) |
| Latest release | [![Release](https://img.shields.io/github/v/release/YadeWira/lzfse-ex)](https://github.com/YadeWira/lzfse-ex/releases/latest) |
| Library (encode/decode) | ✅ Stable |
| Command line tool | ✅ Stable |
| Linux x86 / x86-64 | ✅ Tested |
| Linux ARM64 | ✅ Tested |
| Linux ARM 32-bit | ❌ Not supported |
| Windows x86 (Windows XP SP3 or later) | ✅ Tested |
| Windows x86-64 (Windows 7 or later) | ✅ Tested |
| macOS Intel / Apple Silicon | ✅ Tested |
| Big-endian targets | ❌ Not supported |

This is the reference C implementation of the LZFSE compressor introduced in the
[Compression library](https://developer.apple.com/library/mac/documentation/Performance/Reference/Compression/index.html) with OS X 10.11 and iOS 9.

LZFSE is a Lempel-Ziv style data compression algorithm using Finite State Entropy coding.
It targets similar compression rates at higher compression and decompression speed compared to deflate using zlib.

Files
-----
    README.md                             This file ;-)
    Makefile                              Linux / macOS Makefile
    lzfse.xcodeproj                       Xcode project

    src/lzfse.h                           Main LZFSE header
    src/lzfse_tunables.h                  LZFSE encoder configuration
    src/lzfse_internal.h                  LZFSE internal header
    src/lzfse_decode.c                    LZFSE decoder API entry point
    src/lzfse_encode.c                    LZFSE encoder API entry point
    src/lzfse_decode_base.c               LZFSE decoder internal functions
    src/lzfse_encode_base.c               LZFSE encoder internal functions
    src/lzfse_encode_tables.h             LZFSE encoder tables
    
    src/lzfse_fse.h                       FSE entropy encoder/decoder header
    src/lzfse_fse.c                       FSE entropy encoder/decoder functions
    
    src/lzvn_decode_base.h                LZVN decoder
    src/lzvn_decode_base.c
    src/lzvn_encode_base.h                LZVN encoder
    src/lzvn_encode_base.c
    
    src/lzfse_main.c                      Command line tool

    tests/api-tests.c                     Buffer-to-buffer API tests
    tests/round-trip.cmake                Command line roundtrip test
    tests/cli-tests.cmake                 Command line behaviour tests
    scripts/ci-matrix.sh                  Build and test the supported targets

Building on OS X
----------------

    $ xcodebuild install DSTROOT=/tmp/lzfse.dst

Produces the following files in `/tmp/lzfse.dst`:

    usr/local/bin/lzfse                   command line tool
    usr/local/include/lzfse.h             LZFSE library header
    usr/local/lib/liblzfse.a              LZFSE library

Building on Linux
-----------------

Tested on Ubuntu 15.10 with gcc 5.2.1 and clang 3.6.2. Should work on any recent distribution.

    $ make install INSTALL_PREFIX=/tmp/lzfse.dst/usr/local

Produces the following files in `/tmp/lzfse.dst`:

    usr/local/bin/lzfse                   command line tool
    usr/local/include/lzfse.h             LZFSE library header
    usr/local/lib/liblzfse.a              LZFSE library

Building with cmake
-------------------

    $ mkdir build
    $ cd build
    $ cmake ..
    $ make install

Installs the header, library, and command line tool in `/usr/local`.

Supported platforms
-------------------

The library and command line tool support:

    Linux     x86 (32-bit), x86-64, arm64
    Windows   x86 (Windows XP SP3 or later), x86-64 (Windows 7 or later)
    macOS     x86-64 and arm64

MSVC and MinGW-w64 are supported on Windows. Only little-endian targets are
supported; big-endian systems are not (the LZFSE format is little-endian).

Windows XP SP3 x86 is tested by hand: the x86 binaries pass the test suite,
`codec_check` and roundtrips up to 10 MB on a real XP SP3 machine. The x86-64
target requires Windows 7 or later.

On a Linux host, `scripts/ci-matrix.sh` builds and tests every target whose
toolchain is installed: 32-bit builds with `-m32`, ARM64 with the GNU cross
compiler and qemu-user, and Windows builds with MinGW-w64 run under Wine.
Missing toolchains are reported and skipped. The sanitizer build
(AddressSanitizer + UndefinedBehaviorSanitizer) is part of the matrix as well.

    $ ./scripts/ci-matrix.sh
    $ ./scripts/ci-matrix.sh linux-x64 windows-x64   # only these targets

With CMake, `ctest` runs the same tests:

    $ cmake -S . -B build && cmake --build build && ctest --test-dir build

Encoder tuning
--------------

The default encoder is byte-identical to Apple's. `LZFSE_FAST_ENCODE` trades a
little compression ratio for encoding speed; the format is unchanged, so the
streams still decode with Apple's LZFSE, macOS, zpaq-std's ZPAQL decoder, etc.

    cmake -S . -B build -DLZFSE_FAST_ENCODE=ON    # or: make FAST=1

| Dataset | Default | Fast | Change |
|---|---|---|---|
| Mixed corpus, 52 MB (text, image, audio, executables, documents) | 79.60% | 79.78% | +0.2% size, 70 → ~100 MB/s encode |
| Text files, 1.7 MB | 19.54% | 20.00% | +2.3% size, 75 → 102 MB/s |
| Repeated log lines, 40 MB | 5.33% | 5.20% | −2.5% size, 137 → 196 MB/s |
| Zeros, 40 MB | 24.8 KB | 29.4 KB | +18.6% size, 19 → 129 MB/s |

Decoding speed and the decoder are unaffected. The preset uses a hash width of
2 and stops scanning a match beyond the maximum encodable length (2359 bytes).
The output is no longer byte-identical to Apple's, but it is the same format
(cross-decoding verified in both directions).

The individual tunables in `src/lzfse_tunables.h` (`HASH_BITS`, `HASH_WIDTH`,
`GOOD_MATCH`, `LZVN_THRESHOLD`) can also be overridden with `-D` flags, at the
cost of changing the encoder workspace size.

Ecosystem
---------

[zpaq-std](https://github.com/YadeWira/zpaq-std) 1.6.0 and later bundle
`lzfse-ex` 1.0.3 as their LZFSE codec; its `-ma:lzfse` output is byte-for-byte
identical to Apple's encoder.

Bindings
--------

Python: [dimkr/pylzfse](https://github.com/dimkr/pylzfse)

