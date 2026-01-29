#include "rc5x_protocol.h"
#include "esphome/core/log.h"

/*
 * Marantz RC5 extended protocol (RC5x)
 * See https://arduino-irremote.github.io/Arduino-IRremote/ir__RC5__RC6_8hpp_source.html
 */

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.rc5x";

static const uint32_t BIT_TIME_US = 889;
// RC5X sends data in 2 parts separated by pause: Start bit, Field bit, toggle bit and adress first
// 1 + 1 + 1 + 5
static const uint8_t RC5_COMMAND_FIELD_BIT = 1;
static const uint8_t RC5_TOGGLE_BIT = 1;
static const uint8_t RC5_ADDRESS_BITS = 5;
static const uint8_t FIRST_PART_NBITS = 8;
// Then RC5X sends command and extension together after the pause (6+6 bits)
static const uint8_t RC5_COMMAND_BITS = 6;
static const uint8_t RC5_EXT_BITS = 6;
// for a total of:
static const uint8_t NBITS = 20;

void RC5XProtocol::encode(RemoteTransmitData *dst, const RC5XData &data) {
  static bool toggle = false;
  dst->set_carrier_frequency(36000);
  uint64_t out_data = 0;
  uint8_t command = data.command;
  uint8_t extension = data.extension;

  if (data.command >= 64) {
    out_data |= 0b10 << (RC5_TOGGLE_BIT + RC5_ADDRESS_BITS + RC5_COMMAND_BITS + RC5_EXT_BITS);
    command = command - 64;
  } else {
    out_data |= 0b11 << (RC5_TOGGLE_BIT + RC5_ADDRESS_BITS + RC5_COMMAND_BITS + RC5_EXT_BITS);
  }
  out_data |= toggle << (RC5_ADDRESS_BITS + RC5_COMMAND_BITS + RC5_EXT_BITS);
  out_data |= data.address << (RC5_COMMAND_BITS + RC5_EXT_BITS);
  out_data |= command << (RC5_EXT_BITS);
  out_data |= extension & 0x3F;

  u_int8_t pos = 0;
  for (uint64_t mask = 1UL << (NBITS - 1); mask != 0; mask >>= 1) {
    // Send 4 space pause between adress and command + extension
    if (pos == FIRST_PART_NBITS) {
      for (uint8_t i = 4; 0 < i; i--) {
        dst->space(BIT_TIME_US);
      }
    }
    pos++;
    if (out_data & mask) {
      dst->space(BIT_TIME_US);
      dst->mark(BIT_TIME_US);
    } else {
      dst->mark(BIT_TIME_US);
      dst->space(BIT_TIME_US);
    }
  }
  toggle = !toggle;
}
optional<RC5XData> RC5XProtocol::decode(RemoteReceiveData src) {
  RC5XData out{
      .address = 0,
      .command = 0,
      .extension = 0,
  };
  uint8_t field_bit;

  // Step 1: Read field bit (start bit pattern)
  if (src.expect_space(BIT_TIME_US) && src.expect_mark(BIT_TIME_US)) {
    field_bit = 1;
    ESP_LOGVV(TAG, "Field bit: 1");
  } else if (src.expect_space(2 * BIT_TIME_US)) {
    field_bit = 0;
    ESP_LOGVV(TAG, "Field bit: 0");
  } else {
    ESP_LOGVV(TAG, "Field bit read failed");
    return {};
  }

  // Step 2: Read toggle bit
  if (!(((src.expect_space(BIT_TIME_US) || src.peek_space(2 * BIT_TIME_US)) ||
         (src.expect_mark(BIT_TIME_US) || src.peek_mark(2 * BIT_TIME_US))) &&
        (((src.expect_mark(BIT_TIME_US) || src.expect_mark(2 * BIT_TIME_US)) &&
          (src.expect_space(BIT_TIME_US) || src.peek_space(2 * BIT_TIME_US))) ||
         ((src.expect_space(BIT_TIME_US) || src.expect_space(2 * BIT_TIME_US)) &&
          (src.expect_mark(BIT_TIME_US) || src.peek_mark(2 * BIT_TIME_US)))))) {
    ESP_LOGVV(TAG, "Toggle bit read failed");
    return {};
  }
  ESP_LOGVV(TAG, "Toggle bit read successfully");

  // Step 3a: Read address bits (16-13) using normal Manchester pattern
  uint32_t out_data = 0;
  for (int bit = NBITS - 4; bit >= 13; bit--) {
    if ((src.expect_space(BIT_TIME_US) || src.expect_space(2 * BIT_TIME_US)) &&
        (src.expect_mark(BIT_TIME_US) || src.peek_mark(2 * BIT_TIME_US))) {
      out_data |= 0 << bit;
      ESP_LOGVV(TAG, "Bit %d: 0", bit);
    } else if ((src.expect_mark(BIT_TIME_US) || src.expect_mark(2 * BIT_TIME_US)) &&
               (src.expect_space(BIT_TIME_US) || src.peek_space(2 * BIT_TIME_US))) {
      out_data |= 1 << bit;
      ESP_LOGVV(TAG, "Bit %d: 1", bit);
    } else {
      ESP_LOGVV(TAG, "Bit %d read failed", bit);
      return {};
    }
  }

  // Step 3b: Read bit 12 (last address bit) - followed by 4× BIT_TIME_US pause
  // The pause extends the mark duration to ~5× BIT_TIME_US
  if ((src.expect_space(BIT_TIME_US) || src.expect_space(2 * BIT_TIME_US)) &&
      (src.expect_mark(4 * BIT_TIME_US) || src.expect_mark(5 * BIT_TIME_US) || src.expect_mark(6 * BIT_TIME_US))) {
    out_data |= 0 << 12;
    ESP_LOGVV(TAG, "Bit 12: 0");
  } else if ((src.expect_mark(BIT_TIME_US) || src.expect_mark(2 * BIT_TIME_US)) &&
             (src.expect_space(BIT_TIME_US) || src.expect_space(2 * BIT_TIME_US)) &&
             (src.expect_mark(4 * BIT_TIME_US) || src.expect_mark(5 * BIT_TIME_US))) {
    out_data |= 1 << 12;
    ESP_LOGVV(TAG, "Bit 12: 1");
  } else {
    ESP_LOGVV(TAG, "Bit 12 read failed");
    return {};
  }

  // Step 3c: Read remaining bits (11-1) using normal Manchester pattern
  for (int bit = 11; bit >= 1; bit--) {
    if ((src.expect_space(BIT_TIME_US) || src.expect_space(2 * BIT_TIME_US)) &&
        (src.expect_mark(BIT_TIME_US) || src.peek_mark(2 * BIT_TIME_US))) {
      out_data |= 0 << bit;
      ESP_LOGVV(TAG, "Bit %d: 0", bit);
    } else if ((src.expect_mark(BIT_TIME_US) || src.expect_mark(2 * BIT_TIME_US)) &&
               (src.expect_space(BIT_TIME_US) || src.peek_space(2 * BIT_TIME_US))) {
      out_data |= 1 << bit;
      ESP_LOGVV(TAG, "Bit %d: 1", bit);
    } else {
      ESP_LOGVV(TAG, "Bit %d read failed", bit);
      return {};
    }
  }

  // Step 4: Read last bit (bit 0) with simplified pattern, like RC5 does
  if (src.expect_space(BIT_TIME_US) || src.expect_space(2 * BIT_TIME_US)) {
    out_data |= 0;
    ESP_LOGVV(TAG, "Bit 0: 0 (space)");
  } else if (src.expect_mark(BIT_TIME_US) || src.expect_mark(2 * BIT_TIME_US)) {
    out_data |= 1;
    ESP_LOGVV(TAG, "Bit 0: 1 (mark)");
  } else {
    ESP_LOGVV(TAG, "Bit 0 read failed");
    return {};
  }

  ESP_LOGVV(TAG, "All bits read, out_data: 0x%05X", out_data);

  // Step 5: Extract fields from bit stream
  // Bit layout: [17 address bits] [11-6 command] [5-0 extension]
  out.extension = out_data & 0x3F;
  out.command = (out_data >> 6) & 0x3F;
  out.address = (out_data >> 12) & 0x1F;

  // Apply field bit to command
  out.command = out.command + (1 - field_bit) * 64u;

  ESP_LOGVV(TAG, "Decoded complete - Address: 0x%02X, Command: 0x%02X, Extension: 0x%02X", out.address, out.command,
            out.extension);

  return out;
}
void RC5XProtocol::dump(const RC5XData &data) {
  ESP_LOGI(TAG, "Received RC5X (Marantz): address=0x%02X, command=0x%02X, extension=0x%02X", data.address, data.command,
           data.extension);
}

}  // namespace remote_base
}  // namespace esphome
