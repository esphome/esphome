#pragma once

#include <cstddef>
#include <cstdint>

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

// Receives one piece of the chunk; ctx is whatever the caller passed to for_each_chunk_piece()
using ChunkPieceSink = void (*)(void *ctx, const char *piece, size_t len);

// Calls sink for each piece of the chunk after the prefix: the data lines split on \n, \r or
// \r\n with SSE_SEP between them and SSE_SUFFIX after the last (matching ESPAsyncWebServer: a
// trailing line break adds no empty last line, an inner empty line is kept). A null message has
// no data line and no blank line, only the chunk terminator. Out of line on purpose: one copy
// serves the gather list and the tail.
void for_each_chunk_piece(const char *message, size_t message_len, ChunkPieceSink sink, void *ctx);

// Writes the chunk header placeholder, the retry/id/event lines and, with_data, the first
// "data: " into buf. Returns the prefix length; size - 1 or more means the event name did not fit.
size_t build_chunk_prefix(char *buf, size_t size, const char *event, uint32_t id, uint32_t reconnect, bool with_data);

// Fills the header placeholder at the start of buf once the chunk length is known
void write_chunk_header(char *buf, size_t chunk_len);

}  // namespace esphome::web_server_idf
