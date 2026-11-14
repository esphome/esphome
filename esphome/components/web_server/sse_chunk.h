#pragma once

#include <cstddef>
#include <cstdint>

#include "esphome/components/web_server_base/stream_head.h"

namespace esphome::web_server {

// Wire framing of one Server-Sent-Events event inside the chunked /events response:
//   "%08x\r\n" + fields + "data: " line ["\r\ndata: " line]... "\r\n" + "\r\n" + "\r\n"
// The chunk header and the final CRLF (the chunk terminator) do not count toward the
// chunk length; everything between them does.

// HTTP chunk header "%08x\r\n"
constexpr size_t CHUNK_HDR_LEN = 10;
// Chunk terminator, also the only piece of a null message (no data line, no blank line)
extern const char CHUNK_END[];
constexpr size_t CHUNK_END_LEN = 2;
// Between two data lines: the end of one and the prefix of the next
extern const char SSE_SEP[];
constexpr size_t SSE_SEP_LEN = 8;
// End of the last data line, the blank line ending the event, and the chunk terminator
extern const char SSE_SUFFIX[];
constexpr size_t SSE_SUFFIX_LEN = 6;

// Status, Content-Type and headers of the /events response head
// In flash on ESP8266; the stream helpers read them with the _P functions
extern const char SSE_STATUS[];
extern const char SSE_CONTENT_TYPE[];
constexpr size_t SSE_HEADERS_COUNT = 3;
extern const web_server_base::HttpHeader SSE_HEADERS[SSE_HEADERS_COUNT];
// Hand-framed first chunk holding an empty event (one blank line), as httpd_resp_send_chunk() sent it before
extern const char SSE_FIRST_CHUNK[];

// Receives one piece of the chunk; ctx is whatever the caller passed to for_each_chunk_piece()
using ChunkPieceSink = void (*)(void *ctx, const char *piece, size_t len);

// Calls sink for each piece after the prefix: data lines split on \n, \r or \r\n with SSE_SEP
// between them and SSE_SUFFIX after the last (as ESPAsyncWebServer: a trailing break adds no
// empty line, an inner one is kept). A null message yields only the chunk terminator.
void for_each_chunk_piece(const char *message, size_t message_len, ChunkPieceSink sink, void *ctx);

// Writes the chunk header placeholder, the retry/id/event lines and, with_data, the first
// "data: " into buf. Returns the prefix length; size - 1 or more means the event name did not fit.
size_t build_chunk_prefix(char *buf, size_t size, const char *event, uint32_t id, uint32_t reconnect, bool with_data);

// Fills the header placeholder at the start of buf once the chunk length is known
void write_chunk_header(char *buf, size_t chunk_len);

// Copies the prefix and the message's pieces to dst; returns the bytes written
size_t copy_chunk(uint8_t *dst, const char *prefix, size_t prefix_len, const char *message, size_t message_len);

// Tail capacity for need bytes: cap when they fit, else max / 2, then max; 0 when need > max
size_t tail_queue_cap(size_t need, size_t cap, size_t max);

// Moves the unsent bytes [sent, len) of src to the front of dst (dst may be src) and copies the chunk after them.
// Returns the new length.
size_t queue_chunk(uint8_t *dst, const uint8_t *src, size_t len, size_t sent, const char *prefix, size_t prefix_len,
                   const char *message, size_t message_len);

}  // namespace esphome::web_server
