#include "nexa_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *TAG = "remote.nexa";

static const uint8_t NBITS = 32;
static const uint32_t HEADER_HIGH_US = 250;
static const uint32_t HEADER_LOW_US = 2500;
static const uint32_t BIT_ONE_LOW_US = 250;
static const uint32_t BIT_ZERO_LOW_US = 1250;
static const uint32_t BIT_HIGH_US = 250;

void NexaProtocol::encode(RemoteTransmitData *dst, const NexaData &data) {
  dst->set_carrier_frequency(0);
  //dst->reserve(2 + NBITS * 2u + 2);

  ESP_LOGD(TAG, "---transmitting---");

  dst->item(HEADER_HIGH_US, HEADER_LOW_US);

  // Device
  for (uint32_t mask = 1UL << (26 - 1); mask != 0; mask >>= 1) {
    if (data.device & mask) {
      /* '1' => '10' */
      dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
      dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
    } else {
      /* '0' => '01' */
      dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
      dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
    }
  }

  // Group
  if (data.group & 0x01) {
      /* '1' => '10' */
      dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
      dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
  } else {
      /* '0' => '01' */
      dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
      dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
  }

  // State
  if (data.state & 0x01) {
      /* '1' => '10' */
      dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
      dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
  } else {
      /* '0' => '01' */
      dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
      dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
  }

  // Channel
  for (uint8_t mask = 1UL << (4 - 1); mask != 0; mask >>= 1) {
    if (data.channel & mask) {
      /* '1' => '10' */
      dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
      dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
    } else {
      /* '0' => '01' */
      dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
      dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
    }
  }

  // Finale bit (half of the pause)
  dst->mark(BIT_HIGH_US);

}

optional<NexaData> NexaProtocol::decode(RemoteReceiveData src) {
  NexaData out{
      .device = 0,
      .group = 0,
      .state = 0,
      .channel = 0,
  };
  if (!src.expect_item(HEADER_HIGH_US, HEADER_LOW_US)) 
    return {};


  // From: http://tech.jolowe.se/home-automation-rf-protocols/

  // Device
  for (uint8_t i = 0; i < 26; i++) {
    out.device <<= 1UL;
    if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US) && (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US))) {
      /* '1' => '10' */
      out.device |= 0x01;
    } else if (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US) && (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US))) {
      /* '0' => '01' */
      out.device |= 0x00;
    }
  }

  // GROUP
  for (uint8_t i = 0; i < 1; i++) {
    out.group <<= 1UL;
    if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US) && (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US))) {
      /* '1' => '10' */
      out.group |= 0x01;
    } else if (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US) && (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US))) {
      /* '0' => '01' */
      out.group |= 0x00;
    }
  }

  // STATE
  for (uint8_t i = 0; i < 1; i++) {
    out.state <<= 1UL;
    if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US) && (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US))) {
      /* '1' => '10' */
      out.state |= 0x01;
    } else if (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US) && (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US))) {
      /* '0' => '01' */
      out.state |= 0x00;
    }
  }

  // CHANNEL
  for (uint8_t i = 0; i < 4; i++) {
    out.channel <<= 1UL;
    if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US) && (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US))) {
      /* '1' => '10' */
      out.channel |= 0x01;
    } else if (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US) && (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US))) {
      /* '0' => '01' */
      out.channel |= 0x00;
    }
  }
  return out;
}

void NexaProtocol::dump(const NexaData &data) { ESP_LOGD(TAG, "Received NEXA: device=0x%04X group=%d state=%d channel=%d", data.device, data.group, data.state, data.channel); }

}  // namespace remote_base
}  // namespace esphome
