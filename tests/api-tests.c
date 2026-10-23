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
COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// Tests for the buffer-to-buffer API. These cover the cases that used to
// crash or loop forever: empty input, corrupt input, truncated input, and a
// destination buffer that is exactly the size of the decoded output.

#include "lzfse.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
      failures++;                                                              \
    }                                                                          \
  } while (0)

// Deterministic pseudo-random data with some repetitive runs, so that both
// the compressed and the uncompressed encoder paths get exercised.
static void fill_data(uint8_t *p, size_t n, uint32_t seed) {
  uint32_t s = seed ? seed : 1;
  size_t i;
  for (i = 0; i < n; i++) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    p[i] = (i % 5 == 0) ? (uint8_t)('a' + (i % 3)) : (uint8_t)(s >> 24);
  }
}

static void test_roundtrip(size_t n) {
  uint8_t *src = (uint8_t *)malloc(n ? n : 1);
  uint8_t *cmp = (uint8_t *)malloc(n + 12);
  uint8_t *out = (uint8_t *)malloc(n ? n : 1);
  size_t cmp_size, decoded_size;
  int status;

  CHECK(src != NULL && cmp != NULL && out != NULL);
  if (src == NULL || cmp == NULL || out == NULL)
    goto done;

  fill_data(src, n, (uint32_t)n + 1);
  cmp_size = lzfse_encode_buffer(cmp, n + 12, src, n, NULL);
  CHECK(cmp_size != 0);
  // The uncompressed fallback needs n + 8 (header) + 4 (end marker) bytes.
  CHECK(cmp_size <= n + 12);
  if (cmp_size == 0 || cmp_size > n + 12)
    goto done;

  // Decoding into an exactly-sized destination must report success, not
  // "destination full" (this was ambiguous before the status API existed).
  decoded_size = (size_t)-1;
  status = lzfse_decode_buffer_with_status(out, n, cmp, cmp_size, NULL,
                                           &decoded_size);
  CHECK(status == LZFSE_DECODE_STATUS_OK);
  CHECK(decoded_size == n);
  CHECK(n == 0 || memcmp(out, src, n) == 0);

  // The compatibility API must keep its documented return values.
  CHECK(lzfse_decode_buffer(out, n, cmp, cmp_size, NULL) == n);

  if (n > 0) {
    // One byte short must report DST_FULL and the number of bytes written.
    decoded_size = (size_t)-1;
    status = lzfse_decode_buffer_with_status(out, n - 1, cmp, cmp_size, NULL,
                                             &decoded_size);
    CHECK(status == LZFSE_DECODE_STATUS_DST_FULL);
    CHECK(decoded_size == n - 1);
    CHECK(lzfse_decode_buffer(out, n - 1, cmp, cmp_size, NULL) == n - 1);
  }

  // Truncated streams must fail instead of asking for a larger buffer.
  if (cmp_size > 4) {
    size_t cut;
    for (cut = 1; cut <= 4; cut++) {
      decoded_size = (size_t)-1;
      status = lzfse_decode_buffer_with_status(out, n, cmp, cmp_size - cut,
                                               NULL, &decoded_size);
      CHECK(status != LZFSE_DECODE_STATUS_OK);
      CHECK(status != LZFSE_DECODE_STATUS_DST_FULL);
    }
  }

done:
  free(src);
  free(cmp);
  free(out);
}

int main(void) {
  static const size_t sizes[] = {0,     1,     7,     8,      9,
                                 64,    1000,  4096,  65536,  300000};
  size_t i;

  for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
    test_roundtrip(sizes[i]);

  // Empty input: not a valid stream (a stream for an empty payload is 12
  // bytes, not 0).
  {
    uint8_t src[1] = {0};
    uint8_t dst[16];
    size_t decoded_size = (size_t)-1;
    int status = lzfse_decode_buffer_with_status(
        dst, sizeof(dst), src, 0, NULL, &decoded_size);
    CHECK(status == LZFSE_DECODE_STATUS_SRC_EMPTY);
    CHECK(decoded_size == 0);
  }

  // Garbage input must be reported as invalid, without growing the
  // destination buffer or asking for more memory.
  {
    uint8_t garbage[64];
    uint8_t dst[256];
    size_t decoded_size = (size_t)-1;
    int status;
    memset(garbage, 0xff, sizeof(garbage));
    status = lzfse_decode_buffer_with_status(
        dst, sizeof(dst), garbage, sizeof(garbage), NULL, &decoded_size);
    CHECK(status == LZFSE_DECODE_STATUS_ERROR);
    CHECK(decoded_size == 0);
  }

  // Encoding into a destination that is too small must fail cleanly.
  {
    uint8_t src[1024];
    uint8_t dst[4];
    fill_data(src, sizeof(src), 99);
    CHECK(lzfse_encode_buffer(dst, sizeof(dst), src, sizeof(src), NULL) == 0);
  }

  if (failures != 0) {
    fprintf(stderr, "%d API test(s) failed\n", failures);
    return 1;
  }
  printf("API tests passed\n");
  return 0;
}
