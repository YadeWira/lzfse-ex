# Changelog

## 1.0.3 — 2026-10-22

- Tests: fix the link of the bundled `codec_check` test on macOS when
  building without AddressSanitizer. It referenced the ASan death callback
  unconditionally, and on Mach-O an undefined weak function is a link
  error; the reference is now compiled only when ASan is enabled.
- Encoder: avoid a negative shift exponent in `fse_init_encoder_table()`
  when a symbol frequency equals the number of states (`k == 0`), flagged by
  UBSan in zpaq-std's full run. `delta1` is unused in that case (the state
  is always >= S0), so it is set to 0. Encoded output is unchanged
  (verified byte-for-byte on 25 corpus files).

## 1.0.2 — 2026-10-22

- Decoder: fix an 8-byte heap over-read when decoding damaged data.
  `fse_in_checked_flush64()` loaded a full 8 bytes before checking the lower
  bound, even when the refill amount was zero and even when fewer than 8
  bytes remained before the end of the caller's input buffer. It now reads
  only the bytes it consumes and rejects out-of-range bit counts. The 32-bit
  stream (`fse_in_checked_flush32()`) had the same problem, plus an unaligned
  4-byte load; both are fixed. Decoded output for valid streams is unchanged.
  Found by zpaq-std's `codec_check` with ASan; the bug is present in Apple's
  upstream LZFSE as well.
- Tests: bundle zpaq-std's `codec_check` (MIT) under `tests/codec_check/` and
  run it on repository files as a CTest, so the sanitizer builds exercise
  exact-size buffers, output caps and damaged input. This is the test that
  reproduces the bug above.

## 1.0.1 — 2026-10-22

- Decoder: avoid unsigned pointer underflow (undefined behavior) in match
  copies with a short distance. `dst[i - D]` underflows `size_t` when `i < D`,
  which UBSan flags as pointer overflow on mutated inputs (#53). The match
  source pointer is now computed explicitly. Decoded output is unchanged
  (verified with the test suite and 150 corpus files).
- `lzfse.h`: add `LZFSE_EX_VERSION_STRING` as an alias of
  `LZFSE_VERSION_STRING`, for codec listings such as `codecinfo`.
- Build: require CMake 3.5 and call `cmake_minimum_required()` before
  `project()`. CMake 4 refuses projects that require less than 3.5, which
  broke configuration on the macOS 14 CI runner.

## 1.0.0 — 2026-10-22

First release of `lzfse-ex`, a fork of the Apple LZFSE reference
implementation (upstream `lzfse-1.0`) focused on portable, robust builds for
Linux, Windows (Windows 7 or later) and macOS.

### Fixed

- Command line tool: compressing an empty file or an empty pipe caused a
  double free and a crash (`malloc(0)`/`realloc(0)`); it now produces a valid
  12-byte stream that decodes back to an empty file (#45, #41, #51, #61).
- Command line tool: decoding corrupt or truncated input used to double the
  output buffer until `malloc` failed (exhausting memory and, on 32-bit
  systems, potentially hanging the machine). Decoding now fails immediately
  with a diagnostic and exit status 1 (#47, #54).
- Encoder: inputs of 2 GiB to 4 GiB overflowed the internal 32-bit offsets,
  silently degrading compression. Inputs of 64 MiB or more are now encoded in
  256 KiB blocks with the encoder state translated between them, so offsets
  stay in range (#64). A 3.2 GB test input compresses byte-identically after a
  roundtrip.
- Command line tool: 64-bit file offsets on 32-bit POSIX systems and on
  Windows (`_FILE_OFFSET_BITS`, `_stat64`), so files larger than 2 GB can be
  read on x86.
- Command line tool: the encoder output buffer is sized using the documented
  `input + 12` bound instead of `input`, so incompressible input no longer
  needs a retry (#4).
- `%zu` is no longer required with MSVC before Visual Studio 2015
  (`%Iu` is used instead).

### Added

- `lzfse_decode_buffer_with_status()`: a decode entry point that reports
  whether the input was decoded (`LZFSE_DECODE_STATUS_OK`), the destination
  buffer was too small (`DST_FULL`), the input was truncated (`SRC_EMPTY`) or
  invalid (`ERROR`), and returns the number of bytes written. This also
  removes the ambiguity between "decoded exactly `dst_size` bytes" and
  "destination full" (#5, #44).
- `LZFSE_VERSION_*` macros in `lzfse.h`.
- `tests/api-tests.c`: API tests covering empty, corrupt, truncated, and
  exactly-sized destination cases.
- `tests/cli-tests.cmake`: command line tests for empty files, garbage input,
  and roundtrips, with timeouts to catch hangs.
- `scripts/ci-matrix.sh`: builds and tests every target available on a Linux
  host (x86 via `-m32`, ARM via cross compilers and qemu-user, Windows via
  MinGW-w64 and Wine) plus an AddressSanitizer/UndefinedBehaviorSanitizer
  build.
- `.github/workflows/ci.yml`: CI for Linux x64/x86/arm64/armhf, MSVC x64/x86,
  MinGW x64 under Wine, macOS Intel/Apple Silicon, and sanitizers.
- Shared library versioning: `liblzfse.so.1.0.0` with SONAME `liblzfse.so.1`
  on Linux, matching `install_name` on macOS (#60).
- `README.md`: supported platforms section.
