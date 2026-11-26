#include "sse_chunk.h"

#include <cinttypes>
#include <cstring>

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome::web_server {

const char CHUNK_END[] PROGMEM = "\r\n";
const char SSE_SEP[] PROGMEM = "\r\ndata: ";
const char SSE_SUFFIX[] PROGMEM = "\r\n\r\n\r\n";
const char SSE_STATUS[] PROGMEM = "200 OK";
const char SSE_CONTENT_TYPE[] PROGMEM = "text/event-stream";
const char SSE_FIRST_CHUNK[] PROGMEM = "2\r\n\r\n\r\n";
static const char SSE_TRANSFER_ENCODING[] PROGMEM = "Transfer-Encoding";
static const char SSE_CHUNKED[] PROGMEM = "chunked";
static const char SSE_CACHE_CONTROL[] PROGMEM = "Cache-Control";
static const char SSE_NO_CACHE[] PROGMEM = "no-cache";
static const char SSE_CONNECTION[] PROGMEM = "Connection";
static const char SSE_KEEP_ALIVE[] PROGMEM = "keep-alive";
// Word aligned pointers, which ESP8266 reads straight from flash
const web_server_base::HttpHeader SSE_HEADERS[SSE_HEADERS_COUNT] PROGMEM = {
    {SSE_TRANSFER_ENCODING, SSE_CHUNKED},
    {SSE_CACHE_CONTROL, SSE_NO_CACHE},
    {SSE_CONNECTION, SSE_KEEP_ALIVE},
};

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
    len = buf_append_str_p(buf, size, len, event);
    len = buf_append_str(buf, size, len, "\r\n");
  }
  if (with_data) {
    len = buf_append_str(buf, size, len, "data: ");
  }
  return len;
}

size_t copy_chunk(uint8_t *dst, const char *prefix, size_t prefix_len, const char *message, size_t message_len) {
  uint8_t *const start = dst;
  std::memcpy(dst, prefix, prefix_len);
  dst += prefix_len;
  for_each_chunk_piece(
      message, message_len,
      [](void *ctx, const char *piece, size_t len) {
        auto &out = *static_cast<uint8_t **>(ctx);
        web_server_base::stream_memcpy(out, piece, len);
        out += len;
      },
      &dst);
  return dst - start;
}

size_t tail_queue_cap(size_t need, size_t cap, size_t max) {
  if (need > max) {
    return 0;
  }
  if (need <= cap) {
    return cap;
  }
  return need <= max / 2 ? max / 2 : max;
}

size_t queue_chunk(uint8_t *dst, const uint8_t *src, size_t len, size_t sent, const char *prefix, size_t prefix_len,
                   const char *message, size_t message_len) {
  const size_t pending = len - sent;
  std::memmove(dst, src + sent, pending);
  return pending + copy_chunk(dst + pending, prefix, prefix_len, message, message_len);
}

}  // namespace esphome::web_server
