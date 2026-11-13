#pragma once

#include <cstdint>

#include "esphome/core/helpers.h"

namespace esphome::uart {

/// One software serial TX byte as line levels, one bit per slot.
struct SoftwareSerialTxFrame {
  /// The start bit at slot 0, then the frame, then a stop bit that ends the last run
  uint32_t line;
  /// Slot of the last 0; from the edge after it the line stays high through the stop bits
  uint32_t high_from;
  /// Slots right after an edge into a run of equal bits lasting at least `min_run_cycles`, where a pending
  /// interrupt only delays bits that repeat the level
  uint32_t unlock_at;
};

/// `frame` holds `num_bits` data and parity bits, LSB first. `unlock_at` stays 0 unless `find_unlocks`.
ESPHOME_ALWAYS_INLINE inline SoftwareSerialTxFrame software_serial_tx_frame(uint32_t frame, uint32_t num_bits,
                                                                            uint32_t bit_time, uint32_t min_run_cycles,
                                                                            bool find_unlocks) {
  SoftwareSerialTxFrame tx;
  tx.line = frame << 1 | (1u << (num_bits + 1));
  // Slot 0 is the start bit, always 0, so the argument is never 0
  tx.high_from = 31 - __builtin_clz(~tx.line & ((2u << num_bits) - 1));
  tx.unlock_at = 0;
  if (find_unlocks) {
    const uint32_t edges = tx.line ^ (tx.line << 1 | 1);  // the idle line before the start bit is high
    for (uint32_t pos = 0; pos <= tx.high_from; pos++) {
      if (((edges >> pos) & 1) && (__builtin_ctz(edges >> (pos + 1)) + 1) * bit_time >= min_run_cycles)
        tx.unlock_at |= 1u << pos;
    }
  }
  return tx;
}

}  // namespace esphome::uart
