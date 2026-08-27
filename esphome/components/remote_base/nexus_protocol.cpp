#include "nexus_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.nexus";

/**
 * Example Nexus (Digoo) temperature & humidity sensor packet (36 data bits each with 2 pulses, 72 pulses in total):
 *   10000101             | 1       | 0            | 01      | 000010001010 | 1111  | 01000101
 *   address (sensor ID)  | battery | force update | channel | temperature  | -     | humidity
 *
 * channels start from 0, temperature is 12 bit signed scaled by 10, `-`s are constant bits
 *
 * Ref: https://manual.pilight.org/protocols/433.92/weather/nexus.html
 */

static const uint8_t NBITS = 72;

static const uint32_t HEADER_HIGH_US = 500;
static const uint32_t HEADER_LOW_US = 3900;
static const uint32_t BIT_HIGH_US = 500;
static const uint32_t BIT_ZERO_LOW_US = 980;
static const uint32_t BIT_ONE_LOW_US = 1950;

void NexusProtocol::encode(RemoteTransmitData *dst, const NexusData &data) {
  // encode temperature
  int16_t t = int16_t(data.temperature * 10.0f);
  if (t < 0)
    t &= 0x0FFF;

  dst->reserve(NBITS + 2);  // with header of 2 pulses

  // send header
  dst->item(HEADER_HIGH_US, HEADER_LOW_US);

  // send address
  for (uint8_t mask = 1UL << 7; mask != 0; mask >>= 1) {
    if (data.address & mask) {
      dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
    } else {
      dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
    }
  }

  // send battery level ok flag
  if (data.battery_level) {
    dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
  } else {
    dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
  }

  // send force update (forced transmission) flag
  if (data.force_update) {
    dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
  } else {
    dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
  }

  // send channel
  for (uint8_t mask = 1UL << 1; mask != 0; mask >>= 1) {
    if ((data.channel - 1) & mask) {
      dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
    } else {
      dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
    }
  }
  // send temperature
  for (uint16_t mask = 1UL << 11; mask != 0; mask >>= 1) {
    if (t & mask) {
      dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
    } else {
      dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
    }
  }

  // send constant bits 0b1111
  for (uint8_t i = 0; i < 4; i++) {
    dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
  }

  // send humidity
  for (uint8_t mask = 1UL << 7; mask != 0; mask >>= 1) {
    if (data.humidity & mask) {
      dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
    } else {
      dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
    }
  }
}

optional<NexusData> NexusProtocol::decode(RemoteReceiveData src) {
  NexusData out{
      .channel = 0,
      .address = 0,
      .temperature = 0,
      .humidity = 0,
      .battery_level = false,
      .force_update = false,
  };

  // check packet size
  if (src.size() < NBITS)
    return {};
  // check constant bits
  if (!src.peek_item(BIT_HIGH_US, BIT_ONE_LOW_US, 24 * 2) || !src.peek_item(BIT_HIGH_US, BIT_ONE_LOW_US, 25 * 2) ||
      !src.peek_item(BIT_HIGH_US, BIT_ONE_LOW_US, 26 * 2) || !src.peek_item(BIT_HIGH_US, BIT_ONE_LOW_US, 27 * 2))
    return {};

  uint64_t packet = 0;
  for (uint8_t i = 0; i < NBITS / 2; i++) {
    if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US)) {
      packet = (packet << 1) | 1;
    } else if (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
      packet = (packet << 1) | 0;
    } else {
      return {};
    }
  }

  // check constant bits
  if (((packet >> 8) & 0b1111) != 0b1111) {
    return {};
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

}  // namespace remote_base
}  // namespace esphome
