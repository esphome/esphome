#include "panasonic_protocol.h"
#include "esphome/core/log.h"

namespace esphome::remote_base {

ESPHOME_LOG_TAG(TAG, "remote.panasonic");

static constexpr uint32_t HEADER_HIGH_US = 3502;
static constexpr uint32_t HEADER_LOW_US = 1750;
static constexpr uint32_t BIT_HIGH_US = 502;
static constexpr uint32_t BIT_ZERO_LOW_US = 400;
static constexpr uint32_t BIT_ONE_LOW_US = 1244;

static constexpr uint8_t NBITS_ADDRESS = 16;
static constexpr uint8_t NBITS_ADDRESS_2 = 8;
static constexpr uint8_t NBITS_COMMAND = 32;
static constexpr uint8_t NBITS_SHORT = NBITS_ADDRESS + NBITS_COMMAND;
static constexpr uint8_t NBITS_LONG = NBITS_SHORT + NBITS_ADDRESS_2;

static void encode_bits(RemoteTransmitData *dst, uint32_t value, uint8_t nbits) {
  for (uint32_t mask = 1UL << (nbits - 1); mask != 0; mask >>= 1) {
    dst->item(BIT_HIGH_US, (value & mask) ? BIT_ONE_LOW_US : BIT_ZERO_LOW_US);
  }
}

static bool decode_bits(RemoteReceiveData &src, uint32_t &value, uint8_t nbits) {
  value = 0;
  for (uint32_t mask = 1UL << (nbits - 1); mask != 0; mask >>= 1) {
    if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US)) {
      value |= mask;
    } else if (!src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
      return false;
    }
  }
  return true;
}

void PanasonicProtocol::encode(RemoteTransmitData *dst, const PanasonicData &data) {
  if (data.nbits != NBITS_SHORT && data.nbits != NBITS_LONG) {
    ESP_LOGW(TAG, "Unsupported nbits %u, expected 48 or 56", data.nbits);
    return;
  }
  // Header, one item per bit, trailing mark
  dst->reserve(3 + 2 * data.nbits);
  dst->set_carrier_frequency(data.carrier_frequency);
  dst->item(HEADER_HIGH_US, HEADER_LOW_US);
  encode_bits(dst, data.address, NBITS_ADDRESS);
  if (data.nbits == NBITS_LONG) {
    encode_bits(dst, data.address_2, NBITS_ADDRESS_2);
  }
  encode_bits(dst, data.command, NBITS_COMMAND);
  dst->mark(BIT_HIGH_US);
}

optional<PanasonicData> PanasonicProtocol::decode(RemoteReceiveData src) {
  if (!src.expect_item(HEADER_HIGH_US, HEADER_LOW_US))
    return {};

  uint32_t address;
  uint32_t word;
  if (!decode_bits(src, address, NBITS_ADDRESS) || !decode_bits(src, word, NBITS_COMMAND))
    return {};

  PanasonicData out{};
  out.address = address;
  uint32_t tail;
  if (decode_bits(src, tail, NBITS_ADDRESS_2)) {
    out.nbits = NBITS_LONG;
    out.address_2 = word >> 24;
    out.command = (word << 8) | tail;
  } else {
    out.command = word;
  }
  return out;
}

void PanasonicProtocol::dump(const PanasonicData &data) {
  ESP_LOGI(TAG, "Received Panasonic: nbits=%u, address=0x%04X, address_2=0x%02X, command=0x%08" PRIX32, data.nbits,
           data.address, data.address_2, data.command);
}

}  // namespace esphome::remote_base
