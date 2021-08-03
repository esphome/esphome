#include "rdm6300.h"
#include "esphome/core/log.h"

namespace esphome {
namespace rdm6300 {

static const char *const TAG = "rdm6300";

static const uint8_t RDM6300_START_BYTE = 0x02;
static const uint8_t RDM6300_END_BYTE = 0x03;
static const int8_t RDM6300_STATE_WAITING_FOR_START = -1;

void rdm6300::RDM6300Component::loop() {
  while (this->available() > 0) {
    uint8_t data;
    if (!this->read_byte(&data)) {
      ESP_LOGW(TAG, "Reading data from RDM6300 failed!");
      this->status_set_warning();
      return;
    }

    if (this->read_state_ == RDM6300_STATE_WAITING_FOR_START) {
      if (data == RDM6300_START_BYTE) {
        this->read_state_ = 0;
      } else {
        // Not start byte, probably not synced up correctly.
      }
    } else if (this->read_state_ < 12) {
      uint8_t value = (data > '9') ? data - '7' : data - '0';
      if (this->read_state_ % 2 == 0) {
        this->buffer_[this->read_state_ / 2] = value << 4;
      } else {
        this->buffer_[this->read_state_ / 2] += value;
      }
      this->read_state_++;
    } else if (data != RDM6300_END_BYTE) {
      ESP_LOGW(TAG, "Invalid end byte from RDM6300!");
      this->read_state_ = RDM6300_STATE_WAITING_FOR_START;
    } else {
      uint8_t checksum = 0;
      for (uint8_t i = 0; i < 5; i++)
        checksum ^= this->buffer_[i];
      this->read_state_ = RDM6300_STATE_WAITING_FOR_START;
      if (checksum != this->buffer_[5]) {
        ESP_LOGW(TAG, "Checksum from RDM6300 doesn't match! (0x%02X!=0x%02X)", checksum, this->buffer_[5]);
      } else {
        // Valid data
        this->status_clear_warning();
        const uint32_t result = encode_uint32(this->buffer_[1], this->buffer_[2], this->buffer_[3], this->buffer_[4]);
        bool report = result != last_id_;
        for (auto *card : this->cards_) {
          if (card->process(result)) {
            report = false;
          }
        }
        for (auto *trig : this->triggers_)
          trig->process(result);

        if (report) {
          ESP_LOGD(TAG, "Found new tag with ID %u", result);
        }
      }
    }
  }
}

}  // namespace rdm6300
}  // namespace esphome
