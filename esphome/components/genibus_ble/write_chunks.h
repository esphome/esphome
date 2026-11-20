#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

namespace esphome::genibus_ble {

/// Pass data to write() in order, in pieces of at most chunk_size bytes. Stops at the first piece that write()
/// returns false for, and returns whether every piece was written.
template<typename F> bool write_in_chunks(std::span<const uint8_t> data, size_t chunk_size, F &&write) {
  for (size_t offset = 0; offset < data.size(); offset += chunk_size) {
    if (!write(data.subspan(offset, std::min(chunk_size, data.size() - offset))))
      return false;
  }
  return true;
}

}  // namespace esphome::genibus_ble
