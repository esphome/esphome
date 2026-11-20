#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ONE_WIRE_RMT

#include <cstddef>
#include <cstdint>

#include "esphome/components/one_wire/one_wire_bus.h"

namespace esphome::gpio::rmt_one_wire {

/// Same size and alignment as rmt_symbol_word_t, usable without IDF headers.
struct alignas(uint32_t) Symbol {
  uint16_t duration0 : 15;
  uint16_t level0 : 1;
  uint16_t duration1 : 15;
  uint16_t level1 : 1;
};

constexpr size_t max_tx_slots(size_t mem_words) { return mem_words - 1; }
constexpr size_t max_rx_slots(size_t mem_words) { return mem_words - 2; }

/// Values that a bus may override. Ranges are checked in the Python schema, and the defaults there must match.
/// Write 1 Low, Read Low and the reset stay fixed. High times: slot minus Low time.
struct Timing {
  uint8_t write0_low_us{90};
  uint8_t slot_us{110};
  uint8_t read_threshold_us{13};
};

Symbol reset_symbol();
/// Encodes bits [bit_offset, bit_offset + num_bits) of data, LSB first. Returns num_bits.
size_t encode_write_slots(const uint8_t *data, size_t bit_offset, size_t num_bits, Symbol *out,
                          const Timing &timing = {});
size_t encode_read_slots(size_t num_bits, Symbol *out, const Timing &timing = {});
/// Only OK (presence), NO_PRESENCE, BUS_LOW or CAPTURE_ERROR.
one_wire::TransactionStatus decode_reset(const Symbol *capture, size_t count);
/// Stores num_bits at bit_offset of out, LSB first, changing no other bit. False, with out undefined, unless the
/// capture is num_bits Low-then-High symbols, each Low shorter than a slot, followed by end markers only.
bool decode_read_slots(const Symbol *capture, size_t count, size_t num_bits, uint8_t *out, size_t bit_offset,
                       const Timing &timing = {});

}  // namespace esphome::gpio::rmt_one_wire

#endif  // USE_ONE_WIRE_RMT
