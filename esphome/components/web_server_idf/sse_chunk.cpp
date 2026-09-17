#include "sse_chunk.h"

#include <cinttypes>
#include <cstring>

#include "esphome/core/helpers.h"

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
    // Only a \r before the next \n can end this line, so the search stops there instead of
    // rescanning the rest of the message for every line
    const auto *r = static_cast<const char *>(memchr(pos, '\r', n != nullptr ? n - pos : remaining));
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

size_t build_chunk_prefix(char *buf, size_t size, const char *event, uint32_t id, uint32_t reconnect, bool with_data) {
  size_t len = CHUNK_HDR_LEN;
  if (reconnect) {
    len = buf_append_printf(buf, size, len, "retry: %" PRIu32 "\r\n", reconnect);
  }
  if (id) {
    len = buf_append_printf(buf, size, len, "id: %" PRIu32 "\r\n", id);
  }
  if (event && *event) {
    len = buf_append_str(buf, size, len, "event: ");
    len = buf_append_str(buf, size, len, event);
    len = buf_append_str(buf, size, len, "\r\n");
  }
  if (with_data) {
    len = buf_append_str(buf, size, len, "data: ");
  }
  return len;
}

void write_chunk_header(char *buf, size_t chunk_len) {
  // Eight lowercase hex digits; the temp keeps the terminator format_hex_to writes out of buf
  char digits[9];
  format_hex_to(digits, static_cast<uint32_t>(chunk_len));
  std::memcpy(buf, digits, 8);
  buf[8] = '\r';
  buf[9] = '\n';
}

}  // namespace esphome::web_server_idf
