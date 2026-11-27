#include "denon_protocol.h"
#include "esphome/core/log.h"

namespace esphome::remote_base {

ESPHOME_LOG_TAG(TAG, "remote.denon");

// Timings from Denon's published Pronto codes
static constexpr uint32_t CARRIER_FREQUENCY = 38000;
static constexpr uint32_t BIT_MARK_US = 263;
static constexpr uint32_t BIT_ZERO_SPACE_US = 789;
static constexpr uint32_t BIT_ONE_SPACE_US = 1841;
static constexpr uint32_t FRAME_GAP_US = 43572;

static constexpr uint8_t ADDRESS_BITS = 5;
static constexpr uint8_t COMMAND_BITS = 8;
// Each frame: address and command (both least significant bit first), two trailing bits, stop mark
static constexpr uint8_t FRAME_ITEMS = ADDRESS_BITS + COMMAND_BITS + 2 + 1;

static void encode_bits(RemoteTransmitData *dst, uint32_t value, uint8_t nbits) {
  for (uint8_t i = 0; i < nbits; i++) {
    dst->item(BIT_MARK_US, (value >> i) & 1 ? BIT_ONE_SPACE_US : BIT_ZERO_SPACE_US);
  }
}

static void encode_frame(RemoteTransmitData *dst, uint8_t address, uint8_t command, uint8_t trailer) {
  encode_bits(dst, address, ADDRESS_BITS);
  encode_bits(dst, command, COMMAND_BITS);
  encode_bits(dst, trailer, 2);
  dst->mark(BIT_MARK_US);
}

void DenonProtocol::encode(RemoteTransmitData *dst, const DenonData &data) {
  dst->set_carrier_frequency(CARRIER_FREQUENCY);
  dst->reserve(FRAME_ITEMS * 4u);
  encode_frame(dst, data.address, data.command, 0b00);
  dst->space(FRAME_GAP_US);
  // The second frame inverts the command and the trailing bits
  encode_frame(dst, data.address, ~data.command, 0b11);
}

static bool decode_bits(RemoteReceiveData &src, uint8_t nbits, uint32_t &out) {
  out = 0;
  for (uint8_t i = 0; i < nbits; i++) {
    if (src.expect_item(BIT_MARK_US, BIT_ONE_SPACE_US)) {
      out |= 1UL << i;
    } else if (!src.expect_item(BIT_MARK_US, BIT_ZERO_SPACE_US)) {
      return false;
    }
  }
  return true;
}

optional<DenonData> DenonProtocol::decode(RemoteReceiveData src) {
  uint32_t address, command, trailer;
  if (!decode_bits(src, ADDRESS_BITS, address) || !decode_bits(src, COMMAND_BITS, command) ||
      !decode_bits(src, 2, trailer) || !src.expect_mark(BIT_MARK_US))
    return {};
  // Only the first frame of a pair ends in 00; the inverted second frame ends in 11
  if (trailer != 0b00)
    return {};
  return DenonData{
      .address = static_cast<uint8_t>(address),
      .command = static_cast<uint8_t>(command),
  };
}

void DenonProtocol::dump(const DenonData &data) {
  ESP_LOGI(TAG, "Received Denon: address=%u, command=%u", data.address, data.command);
}

}  // namespace esphome::remote_base
