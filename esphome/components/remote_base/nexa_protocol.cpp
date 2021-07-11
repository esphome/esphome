#include "nexa_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *TAG = "remote.nexa";

static const uint8_t NBITS = 32;
static const uint32_t HEADER_HIGH_US = 319;
static const uint32_t HEADER_LOW_US = 2610;
static const uint32_t BIT_ONE_LOW_US = 1000;
static const uint32_t BIT_ZERO_LOW_US = 140;
static const uint32_t BIT_HIGH_US = 319;
static const uint32_t TX_GAP_EXTENSION = 100;


void NexaProtocol::one(RemoteTransmitData *dst) const {
    // '1' => '10'
    dst->item(BIT_HIGH_US, BIT_ONE_LOW_US+TX_GAP_EXTENSION);
    dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US+TX_GAP_EXTENSION);
    //ESP_LOGD(TAG, "1 ");
}

void NexaProtocol::zero(RemoteTransmitData *dst) const {
    // '0' => '01' 
    dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US+TX_GAP_EXTENSION);
    dst->item(BIT_HIGH_US, BIT_ONE_LOW_US+TX_GAP_EXTENSION);
    //ESP_LOGD(TAG, "0 ");
}

void NexaProtocol::sync(RemoteTransmitData *dst) const {
  dst->item(HEADER_HIGH_US, HEADER_LOW_US);
  //ESP_LOGD(TAG, "S ");
}


void NexaProtocol::encode(RemoteTransmitData *dst, const NexaData &data) {
  dst->set_carrier_frequency(0);
  ESP_LOGD(TAG, "Transmit NEXA: device=0x%04X group=%d state=%d channel=%d level=%d", data.device, data.group, data.state, data.channel, data.level);
  // Start with SPACE (better timing if done)
  dst->space(BIT_HIGH_US*5);

  // Send SYNC
  this->sync(dst);

  //ESP_LOGD(TAG, "Device:");
  // Device (26 bits)
  for (int16_t i = 26 - 1; i >= 0; i--) {
    if (data.device & (1 << i))
      this->one(dst);
    else
      this->zero(dst);
  }

  //ESP_LOGD(TAG, "Group:");
  // Group (1 bit)
  if (data.group != 0)
    this->one(dst);
  else
    this->zero(dst);

  //ESP_LOGD(TAG, "State:");
  // State (1 bit)
  if (data.state != 0)
    this->one(dst);
  else
    this->zero(dst);

  //ESP_LOGD(TAG, "Channel:");
  // Channel (4 bits)
  for (int16_t i = 4 - 1; i >= 0; i--) {
    if (data.channel & (1 << i))
      this->one(dst);
    else
      this->zero(dst);
  }

  //ESP_LOGD(TAG, "Level:");
  // Level (4 bits)
  for (int16_t i = 4 - 1; i >= 0; i--) {
    if (data.level & (1 << i))
      this->one(dst);
    else
      this->zero(dst);
  }

  // Send finishing Zero
  this->zero(dst);

  // Send the PAUSE 
  dst->mark(BIT_HIGH_US);
  dst->space(BIT_HIGH_US*40);

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
  H = The first 26 bits are transmitter unique codes, and it is this code that the reciever "learns" to recognize.
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
  if (!src.expect_item(HEADER_HIGH_US, HEADER_LOW_US)) 
    return {};

  // Device
  for (uint8_t i = 0; i < 26; i++) {
    out.device <<= 1UL;
    if (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ONE_LOW_US) && (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ZERO_LOW_US))) {
      // '1' => '10'
      out.device |= 0x01;
    } else if (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ZERO_LOW_US) && (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ONE_LOW_US))) {
      // '0' => '01'
      out.device |= 0x00;
    }
  }

  // GROUP
  for (uint8_t i = 0; i < 1; i++) {
    out.group <<= 1UL;
    if (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ONE_LOW_US) && (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ZERO_LOW_US))) {
      // '1' => '10'
      out.group |= 0x01;
    } else if (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ZERO_LOW_US) && (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ONE_LOW_US))) {
      // '0' => '01'
      out.group |= 0x00;
    }
  }

  // STATE
  for (uint8_t i = 0; i < 1; i++) {
    out.state <<= 1UL;
    if (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ONE_LOW_US) && (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ZERO_LOW_US))) {
      // '1' => '10'
      out.state |= 0x01;
    } else if (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ZERO_LOW_US) && (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ONE_LOW_US))) {
      // '0' => '01'
      out.state |= 0x00;
    }
  }

  // CHANNEL (EE and BB bits)
  for (uint8_t i = 0; i < 4; i++) {
    out.channel <<= 1UL;
    if (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ONE_LOW_US) && (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ZERO_LOW_US))) {
      // '1' => '10'
      out.channel |= 0x01;
    } else if (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ZERO_LOW_US) && (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ONE_LOW_US))) {
      // '0' => '01'
      out.channel |= 0x00;
    }
  }

  // LEVEL
  for (uint8_t i = 0; i < 4; i++) {
    out.level <<= 1UL;
    if (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ONE_LOW_US) && (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ZERO_LOW_US))) {
      // '1' => '10'
      out.level |= 0x01;
    } else if (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ZERO_LOW_US) && (src.expect_pulse_with_gap(BIT_HIGH_US, BIT_ONE_LOW_US))) {
      // '0' => '01'
      out.level |= 0x00;
    }
  }

  //rawDump(src);

  if (out.device == 0)
    return {};

  return out;
}

void NexaProtocol::rawDump(RemoteReceiveData src) {

  char buffer[256];
  uint32_t buffer_offset = 0;
  buffer_offset += sprintf(buffer, "Timings: ");

  for (int32_t i = 0; i < src.size() - 1; i++) {
    const int32_t value = src[i];
    const uint32_t remaining_length = sizeof(buffer) - buffer_offset;
    int written;

    if (i + 1 < src.size() - 1) {
      written = snprintf(buffer + buffer_offset, remaining_length, "%d, ", value);
    } else {
      written = snprintf(buffer + buffer_offset, remaining_length, "%d", value);
    }

    if (written < 0 || written >= int(remaining_length)) {
      // write failed, flush...
      buffer[buffer_offset] = '\0';
      ESP_LOGD(TAG, "%s", buffer);
      buffer_offset = 0;
      written = sprintf(buffer, "  ");
      if (i + 1 < src.size()) {
        written += sprintf(buffer + written, "%d, ", value);
      } else {
        written += sprintf(buffer + written, "%d", value);
      }
    }

    buffer_offset += written;
  }
  if (buffer_offset != 0) {
    ESP_LOGD(TAG, "%s", buffer);
  }

}


void NexaProtocol::dump(const NexaData &data) { 
  ESP_LOGD(TAG, "Received NEXA: device=0x%04X group=%d state=%d channel=%d level=%d", data.device, data.group, data.state, data.channel, data.level); 
}




}  // namespace remote_base
}  // namespace esphome
