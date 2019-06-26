#include "dfplayer.h"
#include "esphome/core/log.h"
#include <string.h>

namespace esphome {
namespace dfplayer {

const uint8_t cmd_reset = 0x0c;
const uint8_t cmd_volume = 0x06;
const uint8_t cmd_playtrack = 0x03;

static const char* TAG = "dfplayer";

void DFPlayerComponent::setup() {
  send_cmd(cmd_reset);
}

void DFPlayerComponent::update() {
}

void DFPlayerComponent::play_track(uint16_t track) {
  this->send_cmd(cmd_playtrack, track);
}

void DFPlayerComponent::send_cmd(uint8_t cmd, uint16_t argument) {
  uint8_t buffer[10] { 0x7e, 0xff, 0x06, cmd, 0x01, argument >> 8, (uint8_t)argument, 0x00, 0x00, 0xef };
  uint16_t checksum = 0;
  for (uint8_t i = 1; i < 7; i++)
    checksum += buffer[i];
  checksum = -checksum;
  buffer[7] = checksum >> 8;
  buffer[8] = (uint8_t)checksum;

  this->write_array(buffer, 10);
}

void DFPlayerComponent::loop() {
  // Read message
  while (this->available()) {
    uint8_t byte;
    this->read_byte(&byte);

    if (this->read_pos_ == DFPLAYER_READ_BUFFER_LENGTH)
      this->read_pos_ = 0;

    ESP_LOGVV(TAG, "Buffer pos: %u %d", this->read_pos_, byte);  // NOLINT

    if (byte >= 0x7F)
      byte = '?';  // need to be valid utf8 string for log functions.
    this->read_buffer_[this->read_pos_] = byte;

  }
}

}  // namespace dfplayer
}  // namespace esphome
