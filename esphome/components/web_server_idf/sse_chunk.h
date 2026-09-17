#pragma once

#include <cstddef>
#include <cstring>

namespace esphome::web_server_idf {

// Wire framing of one Server-Sent-Events event inside the chunked /events response:
//   "%08x\r\n" + fields + "data: " line ["\r\ndata: " line]... "\r\n" + "\r\n" + "\r\n"
// The chunk header and the final CRLF (the chunk terminator) do not count toward the
// chunk length; everything between them does.

// HTTP chunk header "%08x\r\n"
constexpr size_t CHUNK_HDR_LEN = 10;
// Chunk terminator, also the only piece of a null message (no data line, no blank line)
constexpr char CHUNK_END[] = "\r\n";
constexpr size_t CHUNK_END_LEN = sizeof(CHUNK_END) - 1;
// Between two data lines: the end of one and the prefix of the next
constexpr char SSE_SEP[] = "\r\ndata: ";
constexpr size_t SSE_SEP_LEN = sizeof(SSE_SEP) - 1;
// End of the last data line, the blank line ending the event, and the chunk terminator
constexpr char SSE_SUFFIX[] = "\r\n\r\n\r\n";
constexpr size_t SSE_SUFFIX_LEN = sizeof(SSE_SUFFIX) - 1;

// Calls sink(ptr, len) for each piece of the chunk after the prefix: the data lines split on
// \n, \r or \r\n with SSE_SEP between them and SSE_SUFFIX after the last (matching
// ESPAsyncWebServer: a trailing line break adds no empty last line, an inner empty line is
// kept). A null message has no data line and no blank line, only the chunk terminator.
template<typename Sink> void for_each_chunk_piece(const char *message, size_t message_len, Sink &&sink) {
  if (message == nullptr) {
    sink(CHUNK_END, CHUNK_END_LEN);
    return;
  }
  const char *pos = message;
  const char *end = message + message_len;
  for (;;) {
    const size_t remaining = end - pos;
    const auto *n = static_cast<const char *>(memchr(pos, '\n', remaining));
    const auto *r = static_cast<const char *>(memchr(pos, '\r', remaining));
    if (n == nullptr && r == nullptr) {
      sink(pos, remaining);
      break;
    }
    const char *brk = (r != nullptr && (n == nullptr || r < n)) ? r : n;
    sink(pos, brk - pos);
    pos = brk + ((brk == r && brk + 1 == n) ? 2 : 1);
    if (pos >= end) {
      break;
    }
    sink(SSE_SEP, SSE_SEP_LEN);
  }
  sink(SSE_SUFFIX, SSE_SUFFIX_LEN);
}

}  // namespace esphome::web_server_idf
