#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "esphome/core/helpers.h"

// HTTP chunked transfer framing, shared by the /events stream and other chunked responses
namespace esphome::web_server_base {

// Chunk header "%08x\r\n"
constexpr size_t CHUNK_HDR_LEN = 10;
// Chunk terminator
constexpr char CHUNK_END[] = "\r\n";
constexpr size_t CHUNK_END_LEN = sizeof(CHUNK_END) - 1;

// Fills the header placeholder at the start of buf once the chunk length is known
inline void write_chunk_header(char *buf, size_t chunk_len) {
  // Eight lowercase hex digits; the temp keeps the terminator format_hex_to writes out of buf
  char digits[9];
  format_hex_to(digits, static_cast<uint32_t>(chunk_len));
  std::memcpy(buf, digits, 8);
  buf[8] = '\r';
  buf[9] = '\n';
}

}  // namespace esphome::web_server_base
