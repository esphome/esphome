#include "nexus_protocol.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cinttypes>
#include <cmath>

namespace esphome::remote_base {

ESPHOME_LOG_TAG(TAG, "remote.nexus");

/**
 * Example Nexus (Digoo) temperature & humidity sensor packet (36 data bits, each a mark and a space):
 *   10000101             | 1       | 0            | 01      | 000010001010 | 1111  | 01000101
 *   address (sensor ID)  | battery | force update | channel | temperature  | -     | humidity
 *
 * The channel is sent 0-based and shown 1-based, temperature is 12 bit signed scaled by 10,
 * `-`s are constant bits.
 *
 * Ref: https://manual.pilight.org/protocols/433.92/weather/nexus.html
 */

static constexpr uint8_t NBITS = 36;

static constexpr uint32_t SYNC_HIGH_US = 500;
static constexpr uint32_t SYNC_LOW_US = 3900;
static constexpr uint32_t BIT_HIGH_US = 500;
static constexpr uint32_t BIT_ZERO_LOW_US = 980;
static constexpr uint32_t BIT_ONE_LOW_US = 1950;

void NexusProtocol::encode(RemoteTransmitData *dst, const NexusData &data) {
  // A templated value may be out of range or NaN (an unavailable sensor); send 0 for NaN
  const float temperature = std::isnan(data.temperature) ? 0.0f : std::clamp(data.temperature, -204.8f, 204.7f);
  const auto t = static_cast<int16_t>(lroundf(temperature * 10.0f));
  const uint64_t packet = uint64_t(data.address) << 28 | uint64_t(data.battery_level) << 27 |
                          uint64_t(data.force_update) << 26 | uint64_t((data.channel - 1) & 0b11) << 24 |
                          uint64_t(t & 0x0FFF) << 12 | 0b1111 << 8 | data.humidity;

  dst->reserve(NBITS * 2 + 2);
  for (int8_t i = NBITS - 1; i >= 0; i--)
    dst->item(BIT_HIGH_US, (packet >> i) & 1 ? BIT_ONE_LOW_US : BIT_ZERO_LOW_US);
  // The sync follows the bits, as the sensors send it: its mark ends the last space, its gap separates repeats
  dst->item(SYNC_HIGH_US, SYNC_LOW_US);
}

optional<NexusData> NexusProtocol::decode(RemoteReceiveData src) {
  // A buffer may start with the previous frame's sync when the receiver did not split on it
  src.expect_item(SYNC_HIGH_US, SYNC_LOW_US);
  if (src.size() < NBITS * 2)
    return {};

  uint64_t packet = 0;
  for (uint8_t i = 0; i < NBITS; i++) {
    packet <<= 1;
    if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US)) {
      packet |= 1;
    } else if (!src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
      return {};
    }
  }
  if (((packet >> 8) & 0xF) != 0xF)
    return {};
  ESP_LOGV(TAG, "Nexus packet: 0x%09" PRIX64, packet);

  return NexusData{
      // The 12 bit field sits at bits 12-23; shift it to the top of an int16_t and back to sign extend.
      .temperature = (static_cast<int16_t>((packet >> 8) & 0xFFF0) >> 4) / 10.0f,
      .address = static_cast<uint8_t>(packet >> 28),
      .battery_level = static_cast<bool>((packet >> 27) & 1),
      .force_update = static_cast<bool>((packet >> 26) & 1),
      .channel = static_cast<uint8_t>(((packet >> 24) & 0b11) + 1),
      .humidity = static_cast<uint8_t>(packet),
  };
}

void NexusProtocol::dump(const NexusData &data) {
  ESP_LOGI(TAG,
           "Received Nexus: address=0x%02X, channel=%u, temperature=%.1f, humidity=%u, battery_level=%d, "
           "force_update=%d",
           data.address, data.channel, data.temperature, data.humidity, data.battery_level, data.force_update);
}

}  // namespace esphome::remote_base
