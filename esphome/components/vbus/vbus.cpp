#include "vbus.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome {
namespace vbus {

static const char *const TAG = "vbus";

void VBus::dump_config() {
  ESP_LOGCONFIG(TAG, "VBus:");
  check_uart_settings(9600);
}

static void septet_spread(uint8_t *data, int start, int count, uint8_t septet) {
  for (int i = 0; i < count; i++, septet >>= 1) {
    if (septet & 1)
      data[start + i] |= 0x80;
  }
}

static bool checksum(const uint8_t *data, int start, int count) {
  uint8_t csum = 0x7f;
  for (int i = 0; i < count; i++)
    csum = (csum - data[start + i]) & 0x7f;
  return csum == 0;
}

void VBus::loop() {
  if (!available())
    return;

  while (available()) {
    uint8_t c;
    read_byte(&c);

    if (c == 0xaa) {
      this->state_ = 1;
      this->buffer_.clear();
      continue;
    }
    if (c & 0x80) {
      this->state_ = 0;
      continue;
    }
    if (this->state_ == 0)
      continue;

    if (this->state_ == 1) {
      this->buffer_.push_back(c);
      if (this->buffer_.size() == 7) {
        this->protocol_ = this->buffer_[4];
        this->source_ = (this->buffer_[3] << 8) + this->buffer_[2];
        this->dest_ = (this->buffer_[1] << 8) + this->buffer_[0];
        this->command_ = (this->buffer_[6] << 8) + this->buffer_[5];
      }
      if ((this->protocol_ == 0x20) && (this->buffer_.size() == 15)) {
        this->state_ = 0;
        if (!checksum(this->buffer_.data(), 0, 15)) {
          ESP_LOGE(TAG, "P2 checksum failed");
          continue;
        }
        septet_spread(this->buffer_.data(), 7, 6, this->buffer_[13]);
        uint16_t id = (this->buffer_[8] << 8) + this->buffer_[7];
        uint32_t value =
            (this->buffer_[12] << 24) + (this->buffer_[11] << 16) + (this->buffer_[10] << 8) + this->buffer_[9];
        ESP_LOGV(TAG, "P1 C%04x %04x->%04x: %04x %04" PRIx32 " (%" PRIu32 ")", this->command_, this->source_,
                 this->dest_, id, value, value);
      } else if ((this->protocol_ == 0x10) && (this->buffer_.size() == 9)) {
        if (!checksum(this->buffer_.data(), 0, 9)) {
          ESP_LOGE(TAG, "P1 checksum failed");
          this->state_ = 0;
          continue;
        }
        this->frames_ = this->buffer_[7];
        if (this->frames_) {
          this->state_ = 2;
          this->cframe_ = 0;
          this->fbcount_ = 0;
          this->buffer_.clear();
        } else {
          this->state_ = 0;
          ESP_LOGD(TAG, "P1 empty message");
        }
      }
      continue;
    }

    if (this->state_ == 2) {
      this->fbytes_[this->fbcount_++] = c;
      if (this->fbcount_ < 6)
        continue;
      this->fbcount_ = 0;
      if (!checksum(this->fbytes_, 0, 6)) {
        ESP_LOGE(TAG, "frame checksum failed");
        continue;
      }
      septet_spread(this->fbytes_, 0, 4, this->fbytes_[4]);
      for (int i = 0; i < 4; i++)
        this->buffer_.push_back(this->fbytes_[i]);
      if (++this->cframe_ < this->frames_)
        continue;
      ESP_LOGV(TAG, "P2 C%04x %04x->%04x: %s", this->command_, this->source_, this->dest_,
               format_hex(this->buffer_).c_str());
      for (auto &listener : this->listeners_)
        listener->on_message(this->command_, this->source_, this->dest_, this->buffer_);
      this->state_ = 0;
      continue;
    }
  }
}

void VBusListener::on_message(uint16_t command, uint16_t source, uint16_t dest, std::vector<uint8_t> &message) {
  if ((this->command_ != 0xffff) && (this->command_ != command))
    return;
  if ((this->source_ != 0xffff) && (this->source_ != source))
    return;
  if ((this->dest_ != 0xffff) && (this->dest_ != dest))
    return;
  this->handle_message(message);
}

}  // namespace vbus
}  // namespace esphome
