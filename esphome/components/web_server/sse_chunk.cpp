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
    // event is not a literal, so it cannot go through buf_append_str (a PSTR() wrapper on ESP8266)
    len = buf_append_printf(buf, size, len, "event: %s\r\n", event);
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

}  // namespace esphome::web_server
