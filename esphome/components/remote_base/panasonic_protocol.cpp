#include "panasonic_protocol.h"
#include "esphome/core/log.h"

namespace esphome::remote_base {

ESPHOME_LOG_TAG(TAG, "remote.panasonic");

static constexpr uint32_t HEADER_HIGH_US = 3502;
static constexpr uint32_t HEADER_LOW_US = 1750;
static constexpr uint32_t BIT_HIGH_US = 502;
static constexpr uint32_t BIT_ZERO_LOW_US = 400;
static constexpr uint32_t BIT_ONE_LOW_US = 1244;

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
  if (data.nbits != 48 && data.nbits != 56) {
    ESP_LOGW(TAG, "Unsupported nbits %u, expected 48 or 56", data.nbits);
    return;
  }
  // Header, one item per bit, trailing mark
  dst->reserve(3 + 2 * data.nbits);
  dst->set_carrier_frequency(data.carrier_frequency);
  dst->item(HEADER_HIGH_US, HEADER_LOW_US);
  encode_bits(dst, data.address, 16);
  if (data.nbits == 56) {
    encode_bits(dst, data.address_2, 8);
  }
  encode_bits(dst, data.command, 32);
  dst->mark(BIT_HIGH_US);
}

optional<PanasonicData> PanasonicProtocol::decode(RemoteReceiveData src) {
  if (!src.expect_item(HEADER_HIGH_US, HEADER_LOW_US))
    return {};

  uint32_t address;
  if (!decode_bits(src, address, 16))
    return {};

  PanasonicData out{};
  out.address = address;

  // Try 56 bits first; a 48-bit frame runs out of bit items before the extra byte plus command decode.
  RemoteReceiveData rest = src;
  uint32_t address_2;
  if (decode_bits(rest, address_2, 8) && decode_bits(rest, out.command, 32)) {
    out.nbits = 56;
    out.address_2 = address_2;
    return out;
  }
  if (!decode_bits(src, out.command, 32))
    return {};
  return out;
}

void PanasonicProtocol::dump(const PanasonicData &data) {
  ESP_LOGI(TAG, "Received Panasonic: nbits=%u, address=0x%04X, address_2=0x%02X, command=0x%08" PRIX32, data.nbits,
           data.address, data.address_2, data.command);
}

}  // namespace esphome::remote_base
