/* codec_check adapter for lzfse (buffer API, as zpaq-std) */
#include "codec_check.h"
#include <stdlib.h>
#include "lzfse.h"
const char* cc_name = "lzfse-ex";
const int cc_levels[] = { 1 };
const int cc_nlevels = 1;
size_t cc_bound(size_t n) { return n + n / 8 + 1024; }
long cc_compress(const void* src, size_t n, void* dst, size_t cap, int level) {
  (void)level;
  void* s = malloc(lzfse_encode_scratch_size()); if (!s) return -1;
  size_t r = lzfse_encode_buffer((uint8_t*)dst, cap, (const uint8_t*)src, n, s);
  free(s); return r > 0 ? (long)r : -1;
}
long cc_decompress(const void* src, size_t n, void* dst, size_t cap) {
  size_t r = lzfse_decode_buffer((uint8_t*)dst, cap, (const uint8_t*)src, n, NULL);
  return (long)r;
}
