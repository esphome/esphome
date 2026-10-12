#include "nexus_protocol.h"
#include "esphome/core/log.h"

namespace esphome::remote_base {

ESPHOME_LOG_TAG(TAG, "remote.nexus");

/**
 * Example Nexus (Digoo) temperature & humidity sensor packet (36 data bits each with 2 pulses, 72 pulses in total):
 *   10000101             | 1       | 0            | 01      | 000010001010 | 1111  | 01000101
 *   address (sensor ID)  | battery | force update | channel | temperature  | -     | humidity
 *
 * channels start from 0, temperature is 12 bit signed scaled by 10, `-`s are constant bits
 *
 * Ref: https://manual.pilight.org/protocols/433.92/weather/nexus.html
 */

static constexpr uint8_t NBITS = 72;

static constexpr uint32_t HEADER_HIGH_US = 500;
static constexpr uint32_t HEADER_LOW_US = 3900;
static constexpr uint32_t BIT_HIGH_US = 500;
static constexpr uint32_t BIT_ZERO_LOW_US = 980;
static constexpr uint32_t BIT_ONE_LOW_US = 1950;

void NexusProtocol::encode(RemoteTransmitData *dst, const NexusData &data) {
  // Same layout decode() parses; the channel is sent 0 based
  const auto t = static_cast<uint16_t>(int16_t(data.temperature * 10.0f));
  const uint64_t packet = uint64_t(data.address) << 28 | uint64_t(data.battery_level) << 27 |
                          uint64_t(data.force_update) << 26 | uint64_t((data.channel - 1) & 0b11) << 24 |
                          uint64_t(t & 0x0FFF) << 12 | 0b1111 << 8 | data.humidity;

  dst->reserve(NBITS + 2);  // with header of 2 pulses
  dst->item(HEADER_HIGH_US, HEADER_LOW_US);
  for (uint64_t mask = 1ULL << 35; mask != 0; mask >>= 1)
    dst->item(BIT_HIGH_US, (packet & mask) ? BIT_ONE_LOW_US : BIT_ZERO_LOW_US);
}

optional<NexusData> NexusProtocol::decode(RemoteReceiveData src) {
  NexusData out{};

  // check packet size
  if (src.size() < NBITS)
    return {};
  // check constant bits early: every protocol tries every received frame
  if (!src.peek_item(BIT_HIGH_US, BIT_ONE_LOW_US, 24 * 2) || !src.peek_item(BIT_HIGH_US, BIT_ONE_LOW_US, 25 * 2) ||
      !src.peek_item(BIT_HIGH_US, BIT_ONE_LOW_US, 26 * 2) || !src.peek_item(BIT_HIGH_US, BIT_ONE_LOW_US, 27 * 2))
    return {};

  uint64_t packet = 0;
  for (uint8_t i = 0; i < NBITS / 2; i++) {
    if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US)) {
      packet = (packet << 1) | 1;
    } else if (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
      packet <<= 1;
    } else {
      return {};
    }
  }

  ESP_LOGV(TAG, "Nexus packet: 0x%09" PRIX64, packet);

  out.channel = ((packet >> 24) & 0b11) + 1;
  out.address = (packet >> 28) & 0xFF;
  int16_t t = (packet >> 12) & 0x0FFF;
  t = 0x0800 & t ? 0xF000 | t : t;
  out.temperature = float(t) / 10.0f;
  out.humidity = packet & 0xFF;
  out.battery_level = (packet >> 27) & 0b1;
  out.force_update = (packet >> 26) & 0b1;

  return out;
}

void NexusProtocol::dump(const NexusData &data) {
  ESP_LOGI(TAG, "Received Nexus: ch=%u, address=%u, temp=%.1f, humi=%u, bat=%d, forced=%d", data.channel, data.address,
           data.temperature, data.humidity, data.battery_level, data.force_update);
}

}  // namespace esphome::remote_base
