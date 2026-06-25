#include "pioneer_wyt_protocol.h"

#include "esphome/core/log.h"

namespace esphome::remote_base {

static const char *const TAG = "remote.pioneer_wyt";

/* These timings are from the tcl112 component, but they're awfully close to what I measured.
 * The protocols are pretty similar as well, probably related. Similar protocol to Mitsubishi
 * also, though with different timing. */
constexpr uint32_t HEADER_MARK_US = 3100;
constexpr uint32_t HEADER_SPACE_US = 1650;
constexpr uint32_t BIT_MARK_US = 500;
constexpr uint32_t BIT_ONE_SPACE_US = 1100;
constexpr uint32_t BIT_ZERO_SPACE_US = 350;

constexpr unsigned int PIONEER_WYT_IR_PACKET_BIT_SIZE = 112;

uint8_t PioneerWytData::calc_cs_(uint8_t checksum_offset) const {
  for (uint8_t i = 0; i < WYT_REMOTE_COMMAND_SIZE - 1; i++) {
    checksum_offset += this->data_[i];
  }
  return checksum_offset;
}

void PioneerWytProtocol::encode(RemoteTransmitData *dst, const PioneerWytData &data) {
  dst->set_carrier_frequency(38000);
  // Header: mark+space + initial bit mark => 3 entries. Each bit adds space+mark => 2 entries.
  dst->reserve(dst->get_data().size() + 3 + PIONEER_WYT_IR_PACKET_BIT_SIZE * 2u);
  dst->mark(HEADER_MARK_US);
  dst->space(HEADER_SPACE_US);
  dst->mark(BIT_MARK_US);
  for (size_t i = 0; i < data.size(); i++) {
    this->encode_byte_(dst, data[i]);
  }
  char hex_buf[format_hex_pretty_size(WYT_REMOTE_COMMAND_SIZE)];
  format_hex_pretty_to(hex_buf, &data[0], data.size());
  ESP_LOGD(TAG, "Transmit PioneerWyt: %s cs: %02X", hex_buf, data[data.size() - 1]);
}

void PioneerWytProtocol::encode_byte_(RemoteTransmitData *dst, uint8_t item) {
  for (uint8_t b = 0; b < 8; b++) {
    if (item & (1UL << b)) {
      dst->space(BIT_ONE_SPACE_US);
    } else {
      dst->space(BIT_ZERO_SPACE_US);
    }
    dst->mark(BIT_MARK_US);
  }
}

PioneerWytData PioneerWytData::make_general(bool power, uint8_t mode, float target_temperature, bool beeper,
                                            bool display, bool eco, bool turbo, bool sleep, bool follow_me,
                                            uint8_t remote_temp, bool up_down_swing, bool left_right_swing) {
  PioneerWytData d;
  d[0] = 0x23;
  d[1] = 0xCB;
  d[2] = 0x26;
  d[3] = PIONEER_WYT_TYPE_GENERAL;
  d[4] = 0x00;

  uint8_t b5 = 0;
  if (power)
    b5 |= (1 << 2);
  if (beeper)
    b5 |= (1 << 5);
  if (!display)
    b5 |= (1 << 6);
  if (eco)
    b5 |= (1 << 7);
  d[5] = b5;

  uint8_t b6 = mode & 0x0F;
  if (turbo)
    b6 |= (1 << 6);
  if (follow_me)
    b6 |= (1 << 7);
  d[6] = b6;

  uint8_t temp_whole = static_cast<uint8_t>(target_temperature);
  d[7] = 31 - temp_whole;

  uint8_t b8 = 0;
  if (turbo) {
    b8 |= 5;
  } else if (sleep) {
    b8 |= 1;
  } else if (mode == PIONEER_WYT_MODE_DRY) {
    b8 |= 2;
  }
  if (up_down_swing) {
    b8 |= (7 << 3);
  }
  d[8] = b8;
  d[9] = 0x00;
  d[10] = 0x00;
  d[11] = remote_temp;

  float temp_fraction = target_temperature - temp_whole;
  bool half_digit = (temp_fraction >= 0.25f && temp_fraction <= 0.75f);
  uint8_t b12 = 0;
  if (half_digit)
    b12 |= (1 << 2);
  if (left_right_swing) {
    b12 |= (1 << 7) | (1 << 3);
  } else {
    b12 |= (1 << 7);
  }
  d[12] = b12;

  d.finalize();
  return d;
}

PioneerWytData PioneerWytData::make_fan(uint8_t fan_speed, bool mute, bool vertical_swing, bool horizontal_swing) {
  PioneerWytData d;
  d[0] = 0x23;
  d[1] = 0xCB;
  d[2] = 0x26;
  d[3] = PIONEER_WYT_TYPE_FAN;
  d[4] = 0x00;

  uint8_t b5 = (1 << 6);
  if (mute)
    b5 |= (1 << 5);
  d[5] = b5;

  uint8_t b6 = (fan_speed & 0x07) << 5;
  d[6] = b6;

  uint8_t b7 = 0;
  if (vertical_swing) {
    b7 |= (4 << 2);
  }
  if (horizontal_swing) {
    b7 |= (4 << 5);
  }
  d[7] = b7;

  d[8] = 0x00;
  d[9] = 0x00;
  d[10] = 0x00;
  d[11] = 0x00;
  d[12] = 0x00;

  d.finalize();
  return d;
}

}  // namespace esphome::remote_base
