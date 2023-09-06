#include "nexa_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.nexa";

static const uint8_t NBITS = 32;
static const uint32_t HEADER_HIGH_US = 319;
static const uint32_t HEADER_LOW_US = 2610;
static const uint32_t BIT_HIGH_US = 319;
static const uint32_t BIT_ONE_LOW_US = 1000;
static const uint32_t BIT_ZERO_LOW_US = 140;

static const uint32_t TX_HEADER_HIGH_US = 250;
static const uint32_t TX_HEADER_LOW_US = TX_HEADER_HIGH_US * 10;
static const uint32_t TX_BIT_HIGH_US = 250;
static const uint32_t TX_BIT_ONE_LOW_US = TX_BIT_HIGH_US * 5;
static const uint32_t TX_BIT_ZERO_LOW_US = TX_BIT_HIGH_US * 1;

void NexaProtocol::one(RemoteTransmitData *dst) const {
  // '1' => '10'
  dst->item(TX_BIT_HIGH_US, TX_BIT_ONE_LOW_US);
  dst->item(TX_BIT_HIGH_US, TX_BIT_ZERO_LOW_US);
}

void NexaProtocol::zero(RemoteTransmitData *dst) const {
  // '0' => '01'
  dst->item(TX_BIT_HIGH_US, TX_BIT_ZERO_LOW_US);
  dst->item(TX_BIT_HIGH_US, TX_BIT_ONE_LOW_US);
}

void NexaProtocol::sync(RemoteTransmitData *dst) const { dst->item(TX_HEADER_HIGH_US, TX_HEADER_LOW_US); }

void NexaProtocol::encode(RemoteTransmitData *dst, const NexaData &data) {
  dst->set_carrier_frequency(0);

  // Send SYNC
  this->sync(dst);

  // Device (26 bits)
  for (int16_t i = 26 - 1; i >= 0; i--) {
    if (data.device & (1 << i)) {
      this->one(dst);
    } else {
      this->zero(dst);
    }
  }

  // Group (1 bit)
  if (data.group != 0) {
    this->one(dst);
  } else {
    this->zero(dst);
  }

  // State (1 bit)
  if (data.state == 2) {
    // Special case for dimmers...send 00 as state
    dst->item(TX_BIT_HIGH_US, TX_BIT_ZERO_LOW_US);
    dst->item(TX_BIT_HIGH_US, TX_BIT_ZERO_LOW_US);
  } else if (data.state == 1) {
    this->one(dst);
  } else {
    this->zero(dst);
  }

  // Channel (4 bits)
  for (int16_t i = 4 - 1; i >= 0; i--) {
    if (data.channel & (1 << i)) {
      this->one(dst);
    } else {
      this->zero(dst);
    }
  }

  // Level (4 bits)
  if (data.state == 2) {
    for (int16_t i = 4 - 1; i >= 0; i--) {
      if (data.level & (1 << i)) {
        this->one(dst);
      } else {
        this->zero(dst);
      }
    }
  }

  // Send finishing Zero
  dst->item(TX_BIT_HIGH_US, TX_BIT_ZERO_LOW_US);
}

optional<NexaData> NexaProtocol::decode(RemoteReceiveData src) {
  NexaData out{
      .device = 0,
      .group = 0,
      .state = 0,
      .channel = 0,
      .level = 0,
  };

  // From: http://tech.jolowe.se/home-automation-rf-protocols/
  // New data: http://tech.jolowe.se/old-home-automation-rf-protocols/
  /*

  SHHHH HHHH HHHH HHHH HHHH HHHH HHGO EE BB DDDD 0 P

  S = Sync bit.
  H = The first 26 bits are transmitter unique codes, and it is this code that the receiver "learns" to recognize.
  G = Group code, set to one for the whole group.
  O = On/Off bit. Set to 1 for on, 0 for off.
  E = Unit to be turned on or off. The code is inverted, i.e. '11' equals 1, '00' equals 4.
  B = Button code. The code is inverted, i.e. '11' equals 1, '00' equals 4.
  D = Dim level bits.
  0 = packet always ends with a zero.
  P = Pause, a 10 ms pause in between re-send.

  Update: First of all the '1' and '0' bit seems to be reversed (and be the same as Jula I protocol below), i.e.

  */

  // Require a SYNC pulse + long gap
  if (!src.expect_pulse_with_gap(HEADER_HIGH_US, HEADER_LOW_US))
    return {};

  // Device
  for (uint8_t i = 0; i < 26; i++) {
    out.device <<= 1UL;
    if (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ONE_LOW_US) &&
        (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ZERO_LOW_US))) {
      // '1' => '10'
      out.device |= 0x01;
    } else if (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ZERO_LOW_US) &&
               (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ONE_LOW_US))) {
      // '0' => '01'
      out.device |= 0x00;
    } else {
      // This should not happen...failed command
      return {};
    }
  }

  // GROUP
  for (uint8_t i = 0; i < 1; i++) {
    out.group <<= 1UL;
    if (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ONE_LOW_US) &&
        (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ZERO_LOW_US))) {
      // '1' => '10'
      out.group |= 0x01;
    } else if (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ZERO_LOW_US) &&
               (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ONE_LOW_US))) {
      // '0' => '01'
      out.group |= 0x00;
    } else {
      // This should not happen...failed command
      return {};
    }
  }

  // STATE
  for (uint8_t i = 0; i < 1; i++) {
    out.state <<= 1UL;

    // Special treatment as we should handle 01, 10 and 00
    // We need to care for the advance made in the expect functions
    // hence take them one at a time so that we do not get out of sync
    // in decoding

    if (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ONE_LOW_US)) {
      // Starts with '1'
      if (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
        // '10' => 1
        out.state |= 0x01;
      } else if (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ONE_LOW_US)) {
        // '11' => NOT OK
        // This case is here to make sure we advance through the correct index
        // This should not happen...failed command
        return {};
      }
    } else if (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
      // Starts with '0'
      if (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ONE_LOW_US)) {
        // '01' => 0
        out.state |= 0x00;
      } else if (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
        // '00' => Special case for dimmer! => 2
        out.state |= 0x02;
      }
    }
  }

  // CHANNEL (EE and BB bits)
  for (uint8_t i = 0; i < 4; i++) {
    out.channel <<= 1UL;
    if (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ONE_LOW_US) &&
        (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ZERO_LOW_US))) {
      // '1' => '10'
      out.channel |= 0x01;
    } else if (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ZERO_LOW_US) &&
               (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ONE_LOW_US))) {
      // '0' => '01'
      out.channel |= 0x00;
    } else {
      // This should not happen...failed command
      return {};
    }
  }

  // Optional to transmit LEVEL data (8 bits more)
  if (int32_t(src.get_index() + 8) >= src.size()) {
    return out;
  }

  // LEVEL
  for (uint8_t i = 0; i < 4; i++) {
    out.level <<= 1UL;
    if (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ONE_LOW_US) &&
        (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ZERO_LOW_US))) {
      // '1' => '10'
      out.level |= 0x01;
    } else if (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ZERO_LOW_US) &&
               (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ONE_LOW_US))) {
      // '0' => '01'
      out.level |= 0x00;
    } else {
      // This should not happen...failed command
      break;
    }
  }

  return out;
}

void NexaProtocol::dump(const NexaData &data) {
  ESP_LOGI(TAG, "Received NEXA: device=0x%04X group=%d state=%d channel=%d level=%d", data.device, data.group,
           data.state, data.channel, data.level);
}

}  // namespace remote_base
}  // namespace esphome
