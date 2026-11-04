#include "toto_protocol.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::remote_base {

ESPHOME_LOG_TAG(TAG, "remote.toto");

static constexpr uint32_t PREAMBLE_HIGH_US = 6000;
static constexpr uint32_t PREAMBLE_LOW_US = 3000;
static constexpr uint32_t BIT_HIGH_US = 600;
static constexpr uint32_t BIT_ONE_LOW_US = 1667;
static constexpr uint32_t BIT_ZERO_LOW_US = 520;
static constexpr uint16_t TOTO_HEADER = 0x0802;
static constexpr uint8_t TOTO_HEADER_BITS = 15;
static constexpr BitTimings BIT_TIMINGS{
    .bit_mark = BIT_HIGH_US, .one_space = BIT_ONE_LOW_US, .zero_space = BIT_ZERO_LOW_US};

// Both the header and the payload are sent least significant bit first; the payload is the code (low byte first)
// followed by a checksum that makes the XOR of all three bytes zero.
void TotoProtocol::encode(RemoteTransmitData *dst, const TotoData &data) {
  uint8_t payload[3] = {static_cast<uint8_t>(data.code), static_cast<uint8_t>(data.code >> 8), 0};
  payload[2] = xor_bytes(payload, 2);
  dst->reserve(2 * (1 + TOTO_HEADER_BITS + sizeof(payload) * 8) + 1);
  dst->set_carrier_frequency(38000);
  dst->item(PREAMBLE_HIGH_US, PREAMBLE_LOW_US);

  for (uint32_t mask = 1; mask < (1u << TOTO_HEADER_BITS); mask <<= 1)
    dst->item(BIT_HIGH_US, (TOTO_HEADER & mask) ? BIT_ONE_LOW_US : BIT_ZERO_LOW_US);

  dst->item_bytes<BIT_TIMINGS>(payload, sizeof(payload));
  dst->mark(BIT_HIGH_US);
}
optional<TotoData> TotoProtocol::decode(RemoteReceiveData src) {
  if (!src.expect_item(PREAMBLE_HIGH_US, PREAMBLE_LOW_US))
    return {};

  for (uint32_t mask = 1; mask < (1u << TOTO_HEADER_BITS); mask <<= 1) {
    if (!src.expect_item(BIT_HIGH_US, (TOTO_HEADER & mask) ? BIT_ONE_LOW_US : BIT_ZERO_LOW_US))
      return {};
  }

  uint8_t payload[3];
  if (!src.expect_bytes<BIT_TIMINGS>(payload, sizeof(payload)) || xor_bytes(payload, sizeof(payload)) != 0)
    return {};
  return TotoData{.code = encode_uint16(payload[1], payload[0]),
                  .rc_code_1 = static_cast<uint8_t>(reverse_bits(payload[0]) >> 4),
                  .rc_code_2 = static_cast<uint8_t>(reverse_bits(payload[0]) & 0x0F),
                  .command = reverse_bits(payload[1])};
}
void TotoProtocol::dump(const TotoData &data) { ESP_LOGI(TAG, "Received Toto code 0x%04X", data.code); }

}  // namespace esphome::remote_base
