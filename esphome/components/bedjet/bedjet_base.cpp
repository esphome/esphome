#include "bedjet_base.h"
#include <cstdio>
#include <cstring>

namespace esphome {
namespace bedjet {

/// Converts a BedJet temp step into degrees Fahrenheit.
float bedjet_temp_to_f(const uint8_t temp) {
  // BedJet temp is "C*2"; to get F, multiply by 0.9 (half 1.8) and add 32.
  return 0.9f * temp + 32.0f;
}

/** Cleans up the packet before sending. */
BedjetPacket *BedjetCodec::clean_packet_() {
  // So far no commands require more than 2 bytes of data.
  assert(this->packet_.data_length <= 2);
  for (int i = this->packet_.data_length; i < 2; i++) {
    this->packet_.data[i] = '\0';
  }
  ESP_LOGV(TAG, "Created packet: %02X, %02X %02X", this->packet_.command, this->packet_.data[0], this->packet_.data[1]);
  return &this->packet_;
}

/** Returns a BedjetPacket that will initiate a BedjetButton press. */
BedjetPacket *BedjetCodec::get_button_request(BedjetButton button) {
  this->packet_.command = CMD_BUTTON;
  this->packet_.data_length = 1;
  this->packet_.data[0] = button;
  return this->clean_packet_();
}

/** Returns a BedjetPacket that will set the device's target `temperature`. */
BedjetPacket *BedjetCodec::get_set_target_temp_request(float temperature) {
  this->packet_.command = CMD_SET_TEMP;
  this->packet_.data_length = 1;
  this->packet_.data[0] = temperature * 2;
  return this->clean_packet_();
}

/** Returns a BedjetPacket that will set the device's target fan speed. */
BedjetPacket *BedjetCodec::get_set_fan_speed_request(const uint8_t fan_step) {
  this->packet_.command = CMD_SET_FAN;
  this->packet_.data_length = 1;
  this->packet_.data[0] = fan_step;
  return this->clean_packet_();
}

/** Returns a BedjetPacket that will set the device's current time. */
BedjetPacket *BedjetCodec::get_set_time_request(const uint8_t hour, const uint8_t minute) {
  this->packet_.command = CMD_SET_TIME;
  this->packet_.data_length = 2;
  this->packet_.data[0] = hour;
  this->packet_.data[1] = minute;
  return this->clean_packet_();
}

/** Decodes the extra bytes that were received after being notified with a partial packet. */
void BedjetCodec::decode_extra(const uint8_t *data, uint16_t length) {
  ESP_LOGV(TAG, "Received extra: %d bytes: %d %d %d %d", length, data[1], data[2], data[3], data[4]);
  uint8_t offset = this->last_buffer_size_;
  if (offset > 0 && length + offset <= sizeof(BedjetStatusPacket)) {
    memcpy(((uint8_t *) (&this->buf_)) + offset, data, length);
    ESP_LOGV(TAG,
             "Extra bytes: skip1=0x%08x, skip2=0x%04x, skip3=0x%02x; update phase=0x%02x, "
             "flags=BedjetFlags <conn=%c, leds=%c, units=%c, mute=%c, others=%02x>",
             this->buf_._skip_1_, this->buf_._skip_2_, this->buf_._skip_3_, this->buf_.update_phase,
             this->buf_.flags & 0x20 ? '1' : '0', this->buf_.flags & 0x10 ? '1' : '0',
             this->buf_.flags & 0x04 ? '1' : '0', this->buf_.flags & 0x01 ? '1' : '0',
             this->buf_.flags & ~(0x20 | 0x10 | 0x04 | 0x01));
  } else {
    ESP_LOGI(TAG, "Could not determine where to append to, last offset=%d, max size=%u, new size would be %d", offset,
             sizeof(BedjetStatusPacket), length + offset);
  }
}

/** Decodes the incoming status packet received on the BEDJET_STATUS_UUID.
 *
 * @return `true` if the packet was decoded and represents a "partial" packet; `false` otherwise.
 */
bool BedjetCodec::decode_notify(const uint8_t *data, uint16_t length) {
  ESP_LOGV(TAG, "Received: %d bytes: %d %d %d %d", length, data[1], data[2], data[3], data[4]);

  if (data[1] == PACKET_FORMAT_V3_HOME && data[3] == PACKET_TYPE_STATUS) {
    this->status_packet_.reset();

    // Clear old buffer
    memset(&this->buf_, 0, sizeof(BedjetStatusPacket));
    // Copy new data into buffer
    memcpy(&this->buf_, data, length);
    this->last_buffer_size_ = length;

    // TODO: validate the packet checksum?
    if (this->buf_.mode >= 0 && this->buf_.mode < 7 && this->buf_.target_temp_step >= 38 &&
        this->buf_.target_temp_step <= 86 && this->buf_.actual_temp_step > 1 && this->buf_.actual_temp_step <= 100 &&
        this->buf_.ambient_temp_step > 1 && this->buf_.ambient_temp_step <= 100) {
      // and save it for the update() loop
      this->status_packet_ = this->buf_;
      return this->buf_.is_partial == 1;
    } else {
      // TODO: log a warning if we detect that we connected to a non-V3 device.
      ESP_LOGW(TAG, "Received potentially invalid packet (len %d):", length);
    }
  } else if (data[1] == PACKET_FORMAT_DEBUG || data[3] == PACKET_TYPE_DEBUG) {
    // We don't actually know the packet format for this. Dump packets to log, in case a pattern presents itself.
    ESP_LOGV(TAG,
             "received DEBUG packet: set1=%01fF, set2=%01fF, air=%01fF;  [7]=%d, [8]=%d, [9]=%d, [10]=%d, [11]=%d, "
             "[12]=%d, [-1]=%d",
             bedjet_temp_to_f(data[4]), bedjet_temp_to_f(data[5]), bedjet_temp_to_f(data[6]), data[7], data[8], data[9],
             data[10], data[11], data[12], data[length - 1]);

    if (this->has_status()) {
      this->status_packet_->ambient_temp_step = data[6];
    }
  } else {
    // TODO: log a warning if we detect that we connected to a non-V3 device.
  }

  return false;
}

}  // namespace bedjet
}  // namespace esphome
