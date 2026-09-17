#include "sse_chunk.h"

#include <cstring>

namespace esphome::web_server_idf {

void for_each_chunk_piece(const char *message, size_t message_len, ChunkPieceSink sink, void *ctx) {
  if (message == nullptr) {
    sink(ctx, CHUNK_END, CHUNK_END_LEN);
    return;
  }
  const char *pos = message;
  const char *end = message + message_len;
  for (;;) {
    const size_t remaining = end - pos;
    const auto *n = static_cast<const char *>(memchr(pos, '\n', remaining));
    const auto *r = static_cast<const char *>(memchr(pos, '\r', remaining));
    if (n == nullptr && r == nullptr) {
      sink(ctx, pos, remaining);
      break;
    }
    const char *brk = (r != nullptr && (n == nullptr || r < n)) ? r : n;
    sink(ctx, pos, brk - pos);
    pos = brk + ((brk == r && brk + 1 == n) ? 2 : 1);
    if (pos >= end) {
      break;
    }
    sink(ctx, SSE_SEP, SSE_SEP_LEN);
  }
  sink(ctx, SSE_SUFFIX, SSE_SUFFIX_LEN);
}

}  // namespace esphome::web_server_idf
