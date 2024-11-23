#include "dfplayer.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dfplayer {

static const char *const TAG = "dfplayer";

void DFPlayer::next() {
  this->ack_set_is_playing_ = true;
  ESP_LOGD(TAG, "Playing next track");
  this->send_cmd_(0x01);
}

void DFPlayer::previous() {
  this->ack_set_is_playing_ = true;
  ESP_LOGD(TAG, "Playing previous track");
  this->send_cmd_(0x02);
}
void DFPlayer::play_mp3(uint16_t file) {
  this->ack_set_is_playing_ = true;
  ESP_LOGD(TAG, "Playing file %d in mp3 folder", file);
  this->send_cmd_(0x12, file);
}

void DFPlayer::play_file(uint16_t file) {
  this->ack_set_is_playing_ = true;
  ESP_LOGD(TAG, "Playing file %d", file);
  this->send_cmd_(0x03, file);
}

void DFPlayer::play_file_loop(uint16_t file) {
  this->ack_set_is_playing_ = true;
  ESP_LOGD(TAG, "Playing file %d in loop", file);
  this->send_cmd_(0x08, file);
}

void DFPlayer::play_folder_loop(uint16_t folder) {
  this->ack_set_is_playing_ = true;
  ESP_LOGD(TAG, "Playing folder %d in loop", folder);
  this->send_cmd_(0x17, folder);
}

void DFPlayer::volume_up() {
  ESP_LOGD(TAG, "Increasing volume");
  this->send_cmd_(0x04);
}

void DFPlayer::volume_down() {
  ESP_LOGD(TAG, "Decreasing volume");
  this->send_cmd_(0x05);
}

void DFPlayer::set_device(Device device) {
  ESP_LOGD(TAG, "Setting device to %d", device);
  this->send_cmd_(0x09, device);
}

void DFPlayer::set_volume(uint8_t volume) {
  ESP_LOGD(TAG, "Setting volume to %d", volume);
  this->send_cmd_(0x06, volume);
}

void DFPlayer::set_eq(EqPreset preset) {
  ESP_LOGD(TAG, "Setting EQ to %d", preset);
  this->send_cmd_(0x07, preset);
}

void DFPlayer::sleep() {
  this->ack_reset_is_playing_ = true;
  ESP_LOGD(TAG, "Putting DFPlayer to sleep");
  this->send_cmd_(0x0A);
}

void DFPlayer::reset() {
  this->ack_reset_is_playing_ = true;
  ESP_LOGD(TAG, "Resetting DFPlayer");
  this->send_cmd_(0x0C);
}

void DFPlayer::start() {
  this->ack_set_is_playing_ = true;
  ESP_LOGD(TAG, "Starting playback");
  this->send_cmd_(0x0D);
}

void DFPlayer::pause() {
  this->ack_reset_is_playing_ = true;
  ESP_LOGD(TAG, "Pausing playback");
  this->send_cmd_(0x0E);
}

void DFPlayer::stop() {
  this->ack_reset_is_playing_ = true;
  ESP_LOGD(TAG, "Stopping playback");
  this->send_cmd_(0x16);
}

void DFPlayer::random() {
  this->ack_set_is_playing_ = true;
  ESP_LOGD(TAG, "Playing random file");
  this->send_cmd_(0x18);
}

void DFPlayer::play_folder(uint16_t folder, uint16_t file) {
  ESP_LOGD(TAG, "Playing file %d in folder %d", file, folder);
  if (folder < 100 && file < 256) {
    this->ack_set_is_playing_ = true;
    this->send_cmd_(0x0F, (uint8_t) folder, (uint8_t) file);
  } else if (folder <= 15 && file <= 3000) {
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

  ESP_LOGV(TAG, "Send Command %#02x arg %#04x", cmd, argument);
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
            switch (argument) {
              case 0x01:
                ESP_LOGE(TAG, "Module is busy or uninitialized");
                break;
              case 0x02:
                ESP_LOGE(TAG, "Module is in sleep mode");
                break;
              case 0x03:
                ESP_LOGE(TAG, "Serial receive error");
                break;
              case 0x04:
                ESP_LOGE(TAG, "Checksum incorrect");
                break;
              case 0x05:
                ESP_LOGE(TAG, "Specified track is out of current track scope");
                this->is_playing_ = false;
                break;
              case 0x06:
                ESP_LOGE(TAG, "Specified track is not found");
                this->is_playing_ = false;
                break;
              case 0x07:
                ESP_LOGE(TAG, "Insertion error (an inserting operation only can be done when a track is being played)");
                break;
              case 0x08:
                ESP_LOGE(TAG, "SD card reading failed (SD card pulled out or damaged)");
                break;
              case 0x09:
                ESP_LOGE(TAG, "Entered into sleep mode");
                this->is_playing_ = false;
                break;
            }
            break;
          case 0x41:
            ESP_LOGV(TAG, "Ack ok");
            this->is_playing_ |= this->ack_set_is_playing_;
            this->is_playing_ &= !this->ack_reset_is_playing_;
            this->ack_set_is_playing_ = false;
            this->ack_reset_is_playing_ = false;
            break;
          case 0x3D:
            ESP_LOGV(TAG, "Playback finished");
            this->is_playing_ = false;
            this->on_finished_playback_callback_.call();
            break;
          default:
            ESP_LOGV(TAG, "Received unknown cmd %#02x arg %#04x", cmd, argument);
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
