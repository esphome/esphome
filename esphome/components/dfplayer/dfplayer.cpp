#include "dfplayer.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dfplayer {

static const char *const TAG = "dfplayer";

void DFPlayer::play_folder(uint16_t folder, uint16_t file) {
  if (folder <= 10 && file <= 1000) {
    this->ack_set_is_playing_ = true;
    this->send_cmd_(0x0F, (uint8_t) folder, (uint8_t) file);
  } else if (folder < 100 && file < 256) {
    this->ack_set_is_playing_ = true;
    this->send_cmd_(0x14, (((uint16_t) folder) << 12) | file);
  } else {
    ESP_LOGE(TAG, "Cannot play folder %d file %d.", folder, file);
  }
}

void DFPlayer::send_cmd_(uint8_t cmd, uint16_t argument) {
  uint8_t buffer[10]{0x7e, 0xff, 0x06, cmd, 0x01, (uint8_t) (argument >> 8), (uint8_t) argument, 0x00, 0x00, 0xef};
  uint16_t checksum = 0;
  for (uint8_t i = 1; i < 7; i++)
    checksum += buffer[i];
  checksum = -checksum;
  buffer[7] = checksum >> 8;
  buffer[8] = (uint8_t) checksum;

  this->sent_cmd_ = cmd;

  ESP_LOGD(TAG, "Send Command %#02x arg %#04x", cmd, argument);
  this->write_array(buffer, 10);
}

void DFPlayer::loop() {
  // Read message
  while (this->available()) {
    uint8_t byte;
    this->read_byte(&byte);

    if (this->read_pos_ == DFPLAYER_READ_BUFFER_LENGTH)
      this->read_pos_ = 0;

    switch (this->read_pos_) {
      case 0:  // Start mark
        if (byte != 0x7E)
          continue;
        break;
      case 1:  // Version
        if (byte != 0xFF) {
          ESP_LOGW(TAG, "Expected Version 0xFF, got %#02x", byte);
          this->read_pos_ = 0;
          continue;
        }
        break;
      case 2:  // Buffer length
        if (byte != 0x06) {
          ESP_LOGW(TAG, "Expected Buffer length 0x06, got %#02x", byte);
          this->read_pos_ = 0;
          continue;
        }
        break;
      case 9:  // End byte
        if (byte != 0xEF) {
          ESP_LOGW(TAG, "Expected end byte 0xEF, got %#02x", byte);
          this->read_pos_ = 0;
          continue;
        }
        // Parse valid received command
        uint8_t cmd = this->read_buffer_[3];
        uint16_t argument = (this->read_buffer_[5] << 8) | this->read_buffer_[6];

        ESP_LOGV(TAG, "Received message cmd: %#02x arg %#04x", cmd, argument);

        switch (cmd) {
          case 0x3A:
            if (argument == 1) {
              ESP_LOGI(TAG, "USB loaded");
            } else if (argument == 2) {
              ESP_LOGI(TAG, "TF Card loaded");
            }
            break;
          case 0x3B:
            if (argument == 1) {
              ESP_LOGI(TAG, "USB unloaded");
            } else if (argument == 2) {
              ESP_LOGI(TAG, "TF Card unloaded");
            }
            break;
          case 0x3F:
            if (argument == 1) {
              ESP_LOGI(TAG, "USB available");
            } else if (argument == 2) {
              ESP_LOGI(TAG, "TF Card available");
            } else if (argument == 3) {
              ESP_LOGI(TAG, "USB, TF Card available");
            }
            break;
          case 0x40:
            ESP_LOGV(TAG, "Nack");
            this->ack_set_is_playing_ = false;
            this->ack_reset_is_playing_ = false;
            if (argument == 6) {
              ESP_LOGV(TAG, "File not found");
              this->is_playing_ = false;
            }
            break;
          case 0x41:
            ESP_LOGV(TAG, "Ack ok");
            this->is_playing_ |= this->ack_set_is_playing_;
            this->is_playing_ &= !this->ack_reset_is_playing_;
            this->ack_set_is_playing_ = false;
            this->ack_reset_is_playing_ = false;
            break;
          case 0x3D:  // Playback finished
            this->is_playing_ = false;
            this->on_finished_playback_callback_.call();
            break;
          default:
            ESP_LOGD(TAG, "Command %#02x arg %#04x", cmd, argument);
        }
        this->sent_cmd_ = 0;
        this->read_pos_ = 0;
        continue;
    }
    this->read_buffer_[this->read_pos_] = byte;
    this->read_pos_++;
  }
}
void DFPlayer::dump_config() {
  ESP_LOGCONFIG(TAG, "DFPlayer:");
  this->check_uart_settings(9600);
}

}  // namespace dfplayer
}  // namespace esphome
