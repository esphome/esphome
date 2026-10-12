#include "rc5x_protocol.h"
#include "esphome/core/log.h"

namespace esphome::remote_base {

ESPHOME_LOG_TAG(TAG, "remote.rc5x");

static constexpr uint32_t BIT_TIME_US = 889;
// Start, field and toggle bits plus the address, then a 4 bit pause, then command and extension
static constexpr uint8_t RC5_TOGGLE_BIT = 1;
static constexpr uint8_t RC5_ADDRESS_BITS = 5;
static constexpr uint8_t FIRST_PART_NBITS = 8;
static constexpr uint8_t RC5_COMMAND_BITS = 6;
static constexpr uint8_t RC5_EXT_BITS = 6;
static constexpr uint8_t NBITS = 20;
static constexpr uint8_t PAUSE_BITS = 4;

void RC5XProtocol::encode(RemoteTransmitData *dst, const RC5XData &data) {
  static bool toggle = false;
  dst->set_carrier_frequency(36000);
  dst->reserve(NBITS * 2 + PAUSE_BITS);
  // Start bit, then the field bit, which is clear for commands 64 to 127
  const uint32_t start_field = data.command >= 64 ? 0b10 : 0b11;
  const uint32_t out_data = start_field << (RC5_TOGGLE_BIT + RC5_ADDRESS_BITS + RC5_COMMAND_BITS + RC5_EXT_BITS) |
                            uint32_t(toggle) << (RC5_ADDRESS_BITS + RC5_COMMAND_BITS + RC5_EXT_BITS) |
                            uint32_t(data.address) << (RC5_COMMAND_BITS + RC5_EXT_BITS) |
                            uint32_t(data.command & 0x3F) << RC5_EXT_BITS | (data.extension & 0x3F);

  for (uint8_t bit = NBITS; bit-- > 0;) {
    if (bit == NBITS - 1 - FIRST_PART_NBITS) {
      for (uint8_t i = 0; i < PAUSE_BITS; i++)
        dst->space(BIT_TIME_US);
    }
    if (out_data & (1UL << bit)) {
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
  uint8_t field_bit;
  if (src.expect_space(BIT_TIME_US) && src.expect_mark(BIT_TIME_US)) {
    field_bit = 1;
  } else if (src.expect_space(2 * BIT_TIME_US)) {
    field_bit = 0;
  } else {
    ESP_LOGVV(TAG, "Field bit read failed");
    return {};
  }

  // Toggle bit, either level
  if (!(((src.expect_space(BIT_TIME_US) || src.peek_space(2 * BIT_TIME_US)) ||
         (src.expect_mark(BIT_TIME_US) || src.peek_mark(2 * BIT_TIME_US))) &&
        (((src.expect_mark(BIT_TIME_US) || src.expect_mark(2 * BIT_TIME_US)) &&
          (src.expect_space(BIT_TIME_US) || src.peek_space(2 * BIT_TIME_US))) ||
         ((src.expect_space(BIT_TIME_US) || src.expect_space(2 * BIT_TIME_US)) &&
          (src.expect_mark(BIT_TIME_US) || src.peek_mark(2 * BIT_TIME_US)))))) {
    ESP_LOGVV(TAG, "Toggle bit read failed");
    return {};
  }

  // One Manchester bit: 0, 1, or -1 when neither pattern matches
  auto read_bit = [&src]() -> int {
    if ((src.expect_space(BIT_TIME_US) || src.expect_space(2 * BIT_TIME_US)) &&
        (src.expect_mark(BIT_TIME_US) || src.peek_mark(2 * BIT_TIME_US)))
      return 0;
    if ((src.expect_mark(BIT_TIME_US) || src.expect_mark(2 * BIT_TIME_US)) &&
        (src.expect_space(BIT_TIME_US) || src.peek_space(2 * BIT_TIME_US)))
      return 1;
    return -1;
  };

  uint32_t out_data = 0;
  for (int bit = NBITS - 4; bit >= 13; bit--) {
    const int value = read_bit();
    if (value < 0) {
      ESP_LOGVV(TAG, "Bit %d read failed", bit);
      return {};
    }
    out_data |= uint32_t(value) << bit;
  }

  // Bit 12, the last address bit, runs into the pause, so its mark lasts about 5 bit times
  if ((src.expect_space(BIT_TIME_US) || src.expect_space(2 * BIT_TIME_US)) &&
      (src.expect_mark(4 * BIT_TIME_US) || src.expect_mark(5 * BIT_TIME_US) || src.expect_mark(6 * BIT_TIME_US))) {
    // 0
  } else if ((src.expect_mark(BIT_TIME_US) || src.expect_mark(2 * BIT_TIME_US)) &&
             (src.expect_space(BIT_TIME_US) || src.expect_space(2 * BIT_TIME_US)) &&
             (src.expect_mark(4 * BIT_TIME_US) || src.expect_mark(5 * BIT_TIME_US))) {
    out_data |= 1 << 12;
  } else {
    ESP_LOGVV(TAG, "Bit 12 read failed");
    return {};
  }

  for (int bit = 11; bit >= 1; bit--) {
    const int value = read_bit();
    if (value < 0) {
      ESP_LOGVV(TAG, "Bit %d read failed", bit);
      return {};
    }
    out_data |= uint32_t(value) << bit;
  }

  // The last bit's second half merges into the trailing gap, like RC5
  if (src.expect_space(BIT_TIME_US) || src.expect_space(2 * BIT_TIME_US)) {
    // 0
  } else if (src.expect_mark(BIT_TIME_US) || src.expect_mark(2 * BIT_TIME_US)) {
    out_data |= 1;
  } else {
    ESP_LOGVV(TAG, "Bit 0 read failed");
    return {};
  }

  // [16-12 address] [11-6 command] [5-0 extension]
  RC5XData out{};
  out.address = (out_data >> 12) & 0x1F;
  out.command = ((out_data >> 6) & 0x3F) + (field_bit ? 0 : 64);
  out.extension = out_data & 0x3F;
  return out;
}

void RC5XProtocol::dump(const RC5XData &data) {
  ESP_LOGI(TAG, "Received RC5X (Marantz): address=0x%02X, command=0x%02X, extension=0x%02X", data.address, data.command,
           data.extension);
}

}  // namespace esphome::remote_base
