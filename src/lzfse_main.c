/*
Copyright (c) 2015-2016, Apple Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1.  Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2.  Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer
    in the documentation and/or other materials provided with the distribution.

3.  Neither the name of the copyright holder(s) nor the names of any contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// LZFSE command line tool

#if !defined(_POSIX_C_SOURCE) || (_POSIX_C_SOURCE < 200112L)
#  undef _POSIX_C_SOURCE
#  define _POSIX_C_SOURCE 200112L
#endif

// On 32-bit POSIX systems, make off_t 64 bits so that files larger than 2 GB
// can be read. Must be defined before any system header is included.
#if !defined(_WIN32) && !defined(_FILE_OFFSET_BITS)
#  define _FILE_OFFSET_BITS 64
#endif

#if defined(_MSC_VER) && _MSC_VER < 1900
#  define LZFSE_SIZE_FMT "%Iu" // MSVC before 2015 does not support %zu
#else
#  define LZFSE_SIZE_FMT "%zu"
#endif

#if defined(_MSC_VER)
#  if !defined(_CRT_NONSTDC_NO_DEPRECATE)
#    define _CRT_NONSTDC_NO_DEPRECATE
#  endif
#  if !defined(_CRT_SECURE_NO_WARNINGS)
#    define _CRT_SECURE_NO_WARNINGS
#  endif
#  if !defined(__clang__)
#    define inline __inline
#  endif
#endif

#include "lzfse.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#if defined(_MSC_VER)
#  include <io.h>
#  include <windows.h>
#else
#  include <sys/time.h>
#  include <unistd.h>
#endif

// Same as realloc(x,s), except x is freed when realloc fails
static inline void *lzfse_reallocf(void *x, size_t s) {
  void *y = realloc(x, s);
  if (y == 0) {
    free(x);
    return 0;
  }
  return y;
}

static double get_time() {
#if defined(_MSC_VER)
  LARGE_INTEGER count, freq;
  if (QueryPerformanceFrequency(&freq) && QueryPerformanceCounter(&count)) {
    return (double)count.QuadPart / (double)freq.QuadPart;
  }
  return 1.0e-3 * (double)GetTickCount();
#else
  struct timeval tv;
  if (gettimeofday(&tv, 0) != 0) {
    perror("gettimeofday");
    exit(1);
  }
  return (double)tv.tv_sec + 1.0e-6 * (double)tv.tv_usec;
#endif
}

//--------------------

enum { LZFSE_ENCODE = 0, LZFSE_DECODE };

void usage(int argc, char **argv) {
  fprintf(
      stderr,
      "Usage: %s -encode|-decode [-i input_file] [-o output_file] [-h] [-v]\n",
      argv[0]);
}

#define USAGE(argc, argv)                                                      \
  do {                                                                         \
    usage(argc, argv);                                                         \
    exit(0);                                                                   \
  } while (0)
#define USAGE_MSG(argc, argv, ...)                                             \
  do {                                                                         \
    usage(argc, argv);                                                         \
    fprintf(stderr, __VA_ARGS__);                                              \
    exit(1);                                                                   \
  } while (0)

int main(int argc, char **argv) {
  const char *in_file = 0;  // stdin
  const char *out_file = 0; // stdout
  int op = -1;              // invalid op
  int verbosity = 0;        // quiet

  // Parse options
  for (int i = 1; i < argc;) {
    // no args
    const char *a = argv[i++];
    if (strcmp(a, "-h") == 0)
      USAGE(argc, argv);
    if (strcmp(a, "-v") == 0) {
      verbosity++;
      continue;
    }
    if (strcmp(a, "-encode") == 0) {
      op = LZFSE_ENCODE;
      continue;
    }
    if (strcmp(a, "-decode") == 0) {
      op = LZFSE_DECODE;
      continue;
    }

    // one arg
    const char **arg_var = 0;
    if (strcmp(a, "-i") == 0 && in_file == 0)
      arg_var = &in_file;
    else if (strcmp(a, "-o") == 0 && out_file == 0)
      arg_var = &out_file;
    if (arg_var != 0) {
      // Flag is recognized. Check if there is an argument.
      if (i == argc)
        USAGE_MSG(argc, argv, "Error: Missing arg after %s\n", a);
      *arg_var = argv[i++];
      continue;
    }

    USAGE_MSG(argc, argv, "Error: invalid flag %s\n", a);
  }
  if (op < 0)
    USAGE_MSG(argc, argv, "Error: -encode|-decode required\n");

  // Info
  if (verbosity > 0) {
    if (op == LZFSE_ENCODE)
      fprintf(stderr, "LZFSE encode\n");
    if (op == LZFSE_DECODE)
      fprintf(stderr, "LZFSE decode\n");
    fprintf(stderr, "Input: %s\n", in_file ? in_file : "stdin");
    fprintf(stderr, "Output: %s\n", out_file ? out_file : "stdout");
  }

  // Load input
  size_t in_allocated = 0; // allocated in IN
  size_t in_size = 0;      // used in IN
  uint8_t *in = 0;         // input buffer
  int in_fd = -1;          // input file desc

  if (in_file != 0) {
    // If we have a file name, open it, and allocate the exact input size
#if defined(_WIN32)
    struct _stat64 st;
#else
    struct stat st;
#endif
#if defined(_WIN32)
    in_fd = open(in_file, O_RDONLY | O_BINARY);
#else
    in_fd = open(in_file, O_RDONLY);
#endif
    if (in_fd < 0) {
      perror(in_file);
      exit(1);
    }
#if defined(_WIN32)
    if (_fstat64(in_fd, &st) != 0) {
#else
    if (fstat(in_fd, &st) != 0) {
#endif
      perror(in_file);
      exit(1);
    }
    if (st.st_size < 0 || (uint64_t)st.st_size > (uint64_t)SIZE_MAX) {
      fprintf(stderr, "File is too large\n");
      exit(1);
    }
    in_allocated = (size_t)st.st_size;
  } else {
    // Otherwise, read from stdin, and allocate to 1 MB, grow as needed
    in_allocated = 1 << 20;
    in_fd = 0;
#if defined(_WIN32)
    if (setmode(in_fd, O_BINARY) == -1) {
      perror("setmode");
      exit(1);
    }
#endif
  }
  // Never call malloc(0): some allocators return NULL for it, and a zero
  // allocation also cannot be grown by the loop below. An empty input file
  // still needs a valid one-byte buffer.
  if (in_allocated == 0)
    in_allocated = 1;
  in = (uint8_t *)malloc(in_allocated);
  if (in == 0) {
    perror("malloc");
    exit(1);
  }

  while (1) {
    // re-alloc if needed
    if (in_size == in_allocated) {
      size_t new_allocated;
      if (in_allocated < (100 << 20))
        new_allocated = in_allocated * 2; // double it
      else
        new_allocated = in_allocated + (100 << 20); // or add 100 MB
      if (new_allocated <= in_allocated) {
        fprintf(stderr, "Input is too large\n");
        exit(1);
      }
      in_allocated = new_allocated;
      in = lzfse_reallocf(in, in_allocated);
      if (in == 0) {
        perror("malloc");
        exit(1);
      }
    }

    ptrdiff_t r = read(in_fd, in + in_size, in_allocated - in_size);
    if (r < 0) {
      perror("read");
      exit(1);
    }
    if (r == 0)
      break; // end of file
    in_size += (size_t)r;
  }

  if (in_file != 0) {
    close(in_fd);
    in_fd = -1;
  }

  // Size info
  if (verbosity > 0) {
    fprintf(stderr, "Input size: " LZFSE_SIZE_FMT " B\n", in_size);
  }

  //  Encode/decode
  //  For encoding, LZFSE can fall back to storing the input uncompressed,
  //  which needs the input size plus an 8-byte block header and a 4-byte
  //  end-of-stream marker. For decoding there is no size hint in the stream,
  //  so start with 4x the input and grow on LZFSE_DECODE_STATUS_DST_FULL.
  size_t out_allocated;
  if (op == LZFSE_ENCODE) {
    if (in_size > SIZE_MAX - 12) {
      fprintf(stderr, "Input is too large\n");
      exit(1);
    }
    out_allocated = in_size + 12;
  } else {
    out_allocated = in_size > SIZE_MAX / 4 ? SIZE_MAX / 2 : 4 * in_size;
    if (out_allocated == 0)
      out_allocated = 1; // never call malloc(0)
  }
  size_t out_size = 0;
  size_t aux_allocated = (op == LZFSE_ENCODE) ? lzfse_encode_scratch_size()
                                              : lzfse_decode_scratch_size();
  void *aux = aux_allocated ? malloc(aux_allocated) : 0;
  if (aux_allocated != 0 && aux == 0) {
    perror("malloc");
    exit(1);
  }
  uint8_t *out = (uint8_t *)malloc(out_allocated);
  if (out == 0) {
    perror("malloc");
    exit(1);
  }

  double c0 = get_time();
  while (1) {
    if (op == LZFSE_ENCODE) {
      out_size = lzfse_encode_buffer(out, out_allocated, in, in_size, aux);
      if (out_size != 0)
        break;
    } else {
      size_t decoded_size = 0;
      int status = lzfse_decode_buffer_with_status(
          out, out_allocated, in, in_size, aux, &decoded_size);
      if (status == LZFSE_DECODE_STATUS_OK) {
        out_size = decoded_size;
        break;
      }
      if (status == LZFSE_DECODE_STATUS_SRC_EMPTY) {
        fprintf(stderr, "Decode failed: input is truncated\n");
        exit(1);
      }
      if (status != LZFSE_DECODE_STATUS_DST_FULL) {
        fprintf(stderr, "Decode failed: input is not a valid LZFSE stream\n");
        exit(1);
      }
    }

    // Output buffer was too small: grow and retry. Note that an encode
    // failure (return value 0) is also retried here; the initial buffer is
    // already large enough for the uncompressed fallback, so this only
    // triggers for unusual inputs.
    if (verbosity > 0)
      fprintf(stderr, "Output buffer was too small, increasing size...\n");
    if (out_allocated > SIZE_MAX / 2) {
      fprintf(stderr, "Output is too large\n");
      exit(1);
    }
    out_allocated *= 2;
    out = (uint8_t *)lzfse_reallocf(out, out_allocated);
    if (out == 0) {
      perror("malloc");
      exit(1);
    }
  }
  double c1 = get_time();

  if (verbosity > 0) {
    fprintf(stderr, "Output size: " LZFSE_SIZE_FMT " B\n", out_size);
    size_t raw_size = (op == LZFSE_ENCODE) ? in_size : out_size;
    size_t compressed_size = (op == LZFSE_ENCODE) ? out_size : in_size;
    fprintf(stderr, "Compression ratio: %.3f\n",
            (double)raw_size / (double)compressed_size);
    double ns_per_byte = 1.0e9 * (c1 - c0) / (double)raw_size;
    double mb_per_s = (double)raw_size / 1024.0 / 1024.0 / (c1 - c0);
    fprintf(stderr, "Speed: %.2f ns/B, %.2f MB/s\n",ns_per_byte,mb_per_s);
  }

  // Write output
  int out_fd = -1;
  if (out_file) {
#if defined(_WIN32)
    out_fd = open(out_file, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY,
      S_IWRITE);
#else
    out_fd = open(out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
#endif
    if (out_fd < 0) {
      perror(out_file);
      exit(1);
    }
  } else {
    out_fd = 1; // stdout
#if defined(_WIN32)
    if (setmode(out_fd, O_BINARY) == -1) {
      perror("setmode");
      exit(1);
    }
#endif
  }
  for (size_t out_pos = 0; out_pos < out_size;) {
    ptrdiff_t w = write(out_fd, out + out_pos, out_size - out_pos);
    if (w < 0) {
      perror("write");
      exit(1);
    }
    if (w == 0) {
      fprintf(stderr, "Failed to write to output file\n");
      exit(1);
    }
    out_pos += (size_t)w;
  }
  if (out_file != 0) {
    close(out_fd);
    out_fd = -1;
  }

  free(in);
  free(out);
  free(aux);
  return 0; // OK
}
