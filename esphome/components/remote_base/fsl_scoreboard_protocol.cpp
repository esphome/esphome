#include "fsl_scoreboard_protocol.h"
#include "esphome/core/log.h"

namespace esphome::remote_base {

static const char *const TAG = "remote.fsl_scoreboard";

static constexpr uint32_t BIT_TIME_US = 528;
static constexpr uint8_t SYNC_BITS = 4;

void FSLScoreboardProtocol::encode(RemoteTransmitData *dst, const FSLScoreboardData &data) {
  ESP_LOGD(TAG, "Sending FSL Scoreboard: field=%d, value=%d", data.field, data.value);

  // Extract digits
  uint8_t hundreds = (data.value >= 100) ? (data.value / 100) % 10 : 0xF;
  uint8_t tens = (data.value >= 10) ? (data.value / 10) % 10 : 0xF;
  uint8_t units = data.value % 10;

  // Build 32-bit data: 3 F 2 H 1 T 0 U
  uint32_t payload = 0;
  payload |= (uint32_t) 0x3 << 28;  // Position marker 3
  payload |= (uint32_t) (data.field & 0xF) << 24;
  payload |= (uint32_t) 0x2 << 20;  // Position marker 2
  payload |= (uint32_t) (hundreds & 0xF) << 16;
  payload |= (uint32_t) 0x1 << 12;  // Position marker 1
  payload |= (uint32_t) (tens & 0xF) << 8;
  payload |= (uint32_t) 0x0 << 4;  // Position marker 0
  payload |= (uint32_t) (units & 0xF);

  dst->reserve(38 + 72 * 10);

  // Preamble: 19 pairs of [528µs, -528µs] = 38 bits
  for (int i = 0; i < 19; i++) {
    dst->mark(BIT_TIME_US);
    dst->space(BIT_TIME_US);
  }

  // Send 10 blocks
  for (int block = 0; block < 10; block++) {
    // Sync: 111 (3 bits = 1584µs mark)
    dst->mark(BIT_TIME_US * 3);

    // Manchester encode 32 bits (MSB first)
    for (int i = 31; i >= 0; i--) {
      if (payload & (1U << i)) {
        // 1 = 01 (space then mark)
        dst->space(BIT_TIME_US);
        dst->mark(BIT_TIME_US);
      } else {
        // 0 = 10 (mark then space)
        dst->mark(BIT_TIME_US);
        dst->space(BIT_TIME_US);
      }
    }

    // 33rd Manchester bit (encode as 0 = 10)
    dst->mark(BIT_TIME_US);
    dst->space(BIT_TIME_US);

    // Postamble: 000 (3 bits = 1584µs space)
    dst->space(BIT_TIME_US * 3);
  }
}

optional<FSLScoreboardData> FSLScoreboardProtocol::decode(RemoteReceiveData src) {
  ESP_LOGVV(TAG, "Decode attempt, data size: %d, first items: [%d, %d, %d, %d]", src.size(), src.peek(0), src.peek(1),
            src.peek(2), src.peek(3));

  // Skip to first mark if we start with a space
  if (src.peek(0) < 0)
    src.advance(1);

  // Look for preamble: at least 15 alternating 528µs mark/space pairs
  int preamble_count = 0;
  while (src.peek_item(BIT_TIME_US, BIT_TIME_US)) {
    src.advance(2);  // Advance by 2 since peek_item checks mark+space
    preamble_count++;
  }

  if (preamble_count < 15) {
    ESP_LOGVV(TAG, "Preamble too short: %d, next items: [%d, %d, %d, %d]", preamble_count, src.peek(0), src.peek(1),
              src.peek(2), src.peek(3));
    return {};
  }

  ESP_LOGVV(TAG, "Found preamble: %d pairs", preamble_count);

  // Try to decode up to 10 blocks
  for (int block = 0; block < 10 && src.peek(0) != 0; block++) {
    // Scan for sync mark (3-bit '111', may merge with first Manchester bit)
    int scanned = 0;
    while (src.peek(0) != 0) {
      int32_t sync_mark = src.peek(0);

      if (sync_mark > 0) {
        int sync_bits = (sync_mark + BIT_TIME_US / 2) / BIT_TIME_US;
        if (sync_bits >= 3 && sync_bits <= 4) {
          // Found valid sync
          ESP_LOGVV(TAG, "Block %d: found sync at offset %d: %d (%d bits)", block, scanned, sync_mark, sync_bits);
          break;
        }
      }

      // Not a sync, keep scanning
      src.advance(1);
      scanned++;
    }

    if (src.peek(0) == 0) {
      ESP_LOGVV(TAG, "No more data after scanning %d items", scanned);
      break;
    }

    int32_t sync_mark = src.peek(0);
    int sync_bits = (sync_mark + BIT_TIME_US / 2) / BIT_TIME_US;

    // Found potential sync, try to decode this block

    src.advance(1);

    // Decode 32 data bits from Manchester pairs
    uint32_t payload = 0;
    int pending_bit = (sync_bits == 4) ? 1 : -1;
    int pending_count = (sync_bits == 4) ? 1 : 0;
    bool decode_failed = false;

    for (int data_bit = 0; data_bit < 32; data_bit++) {
      int first_bit, second_bit;

      // Get first Manchester bit
      if (pending_count > 0) {
        first_bit = pending_bit;
        pending_count--;
      } else {
        int32_t val = src.peek(0);
        if (val == 0) {
          ESP_LOGVV(TAG, "Block %d: ran out of data getting first Manchester bit at data bit %d", block, data_bit);
          decode_failed = true;
          break;
        }
        int num_bits = (abs(val) + BIT_TIME_US / 2) / BIT_TIME_US;
        first_bit = (val > 0) ? 1 : 0;
        pending_bit = first_bit;
        pending_count = num_bits - 1;
        src.advance(1);
      }

      // Get second Manchester bit
      if (pending_count > 0) {
        second_bit = pending_bit;
        pending_count--;
      } else {
        int32_t val = src.peek(0);
        if (val == 0) {
          ESP_LOGVV(TAG, "Block %d: ran out of data getting second Manchester bit at data bit %d", block, data_bit);
          decode_failed = true;
          break;
        }
        int num_bits = (abs(val) + BIT_TIME_US / 2) / BIT_TIME_US;
        second_bit = (val > 0) ? 1 : 0;
        pending_bit = second_bit;
        pending_count = num_bits - 1;
        src.advance(1);
      }

      // Decode Manchester pair: 10=0, 01=1
      if (first_bit == 1 && second_bit == 0) {
        payload = (payload << 1) | 0;
      } else if (first_bit == 0 && second_bit == 1) {
        payload = (payload << 1) | 1;
      } else {
        ESP_LOGVV(TAG, "Block %d: invalid Manchester pair at data bit %d: %d%d", block, data_bit, first_bit,
                  second_bit);
        decode_failed = true;
        break;
      }
    }

    if (decode_failed) {
      ESP_LOGVV(TAG, "Block %d: Manchester decode failed", block);
      src.advance(1);
      continue;
    }

    // Validate 33rd Manchester bit (must be valid pair: 10 or 01)
    int bit_33_first, bit_33_second;

    // Get first bit
    if (pending_count > 0) {
      bit_33_first = pending_bit;
      pending_count--;
    } else {
      int32_t val = src.peek(0);
      if (val == 0) {
        ESP_LOGVV(TAG, "Block %d: missing 33rd bit", block);
        decode_failed = true;
      } else {
        int num_bits = (abs(val) + BIT_TIME_US / 2) / BIT_TIME_US;
        bit_33_first = (val > 0) ? 1 : 0;
        pending_bit = bit_33_first;
        pending_count = num_bits - 1;
        src.advance(1);
      }
    }

    // Get second bit
    if (!decode_failed) {
      if (pending_count > 0) {
        bit_33_second = pending_bit;
        pending_count--;
      } else {
        int32_t val = src.peek(0);
        if (val == 0) {
          ESP_LOGVV(TAG, "Block %d: missing 33rd bit second half", block);
          decode_failed = true;
        } else {
          int num_bits = (abs(val) + BIT_TIME_US / 2) / BIT_TIME_US;
          bit_33_second = (val > 0) ? 1 : 0;
          pending_bit = bit_33_second;
          pending_count = num_bits - 1;
          src.advance(1);
        }
      }
    }

    // Validate it's a valid Manchester pair
    if (!decode_failed) {
      if ((bit_33_first != 1 || bit_33_second != 0) && (bit_33_first != 0 || bit_33_second != 1)) {
        ESP_LOGVV(TAG, "Block %d: invalid 33rd Manchester pair: %d%d", block, bit_33_first, bit_33_second);
        decode_failed = true;
      }
    }

    if (decode_failed) {
      // Clear pending bits to avoid misalignment in next block
      continue;
    }

    // Validate postamble: at least 3 space bits
    int space_bits = 0;

    // Count pending space bits
    if (pending_count > 0 && pending_bit == 0) {
      space_bits += pending_count;
    }

    // Read more timings if needed
    while (space_bits < 3 && src.peek(0) != 0) {
      int32_t val = src.peek(0);
      if (val < 0) {
        int num_bits = (abs(val) + BIT_TIME_US / 2) / BIT_TIME_US;
        space_bits += num_bits;
        src.advance(1);
      } else {
        // Hit next sync mark, stop
        break;
      }
    }

    if (space_bits < 3) {
      ESP_LOGVV(TAG, "Block %d: insufficient postamble space: %d bits", block, space_bits);
      src.advance(1);
      continue;
    }

    // Validate position markers
    uint8_t pos3 = (payload >> 28) & 0xF;
    uint8_t field = (payload >> 24) & 0xF;
    uint8_t pos2 = (payload >> 20) & 0xF;
    uint8_t hundreds = (payload >> 16) & 0xF;
    uint8_t pos1 = (payload >> 12) & 0xF;
    uint8_t tens = (payload >> 8) & 0xF;
    uint8_t pos0 = (payload >> 4) & 0xF;
    uint8_t units = payload & 0xF;

    if (pos3 != 0x3 || pos2 != 0x2 || pos1 != 0x1 || pos0 != 0x0) {
      ESP_LOGVV(TAG, "Block %d: invalid position markers: %X %X %X %X", block, pos3, pos2, pos1, pos0);
      src.advance(1);
      continue;
    }

    // Valid block found!
    uint16_t value = 0;
    if (hundreds != 0xF)
      value += hundreds * 100;
    if (tens != 0xF)
      value += tens * 10;
    if (units != 0xF)
      value += units;

    ESP_LOGD(TAG, "Decoded block %d: field=%d, value=%d", block, field, value);
    return FSLScoreboardData{
        .field = field,
        .value = value,
    };
  }

  // No valid blocks found
  return {};
}

void FSLScoreboardProtocol::dump(const FSLScoreboardData &data) {
  ESP_LOGI(TAG, "Received FSL Scoreboard: field=%d, value=%d", data.field, data.value);
}

}  // namespace esphome::remote_base
