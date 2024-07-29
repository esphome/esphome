#include "micronova.h"
#include "esphome/core/log.h"

namespace esphome {
namespace micronova {

void MicroNova::setup() {
  if (this->enable_rx_pin_ != nullptr) {
    this->enable_rx_pin_->setup();
    this->enable_rx_pin_->pin_mode(gpio::FLAG_OUTPUT);
    this->enable_rx_pin_->digital_write(false);
  }
  this->current_transmission_.request_transmission_time = millis();
  this->current_transmission_.memory_location = 0;
  this->current_transmission_.memory_address = 0;
  this->current_transmission_.reply_pending = false;
  this->current_transmission_.initiating_listener = nullptr;
}

void MicroNova::dump_config() {
  ESP_LOGCONFIG(TAG, "MicroNova:");
  if (this->enable_rx_pin_ != nullptr) {
    LOG_PIN("  Enable RX Pin: ", this->enable_rx_pin_);
  }

  for (auto &mv_sensor : this->micronova_listeners_) {
    mv_sensor->dump_config();
    ESP_LOGCONFIG(TAG, "    sensor location:%02X, address:%02X", mv_sensor->get_memory_location(),
                  mv_sensor->get_memory_address());
  }
}

void MicroNova::update() {
  ESP_LOGD(TAG, "Schedule sensor update");
  for (auto &mv_listener : this->micronova_listeners_) {
    mv_listener->set_needs_update(true);
  }
}

void MicroNova::loop() {
  // Only read one sensor that needs update per loop
  // If STOVE_REPLY_DELAY time has passed since last loop()
  // check for a reply from the stove
  if ((this->current_transmission_.reply_pending) &&
      (millis() - this->current_transmission_.request_transmission_time > STOVE_REPLY_DELAY)) {
    int stove_reply_value = this->read_stove_reply();
    if (this->current_transmission_.initiating_listener != nullptr) {
      this->current_transmission_.initiating_listener->process_value_from_stove(stove_reply_value);
      this->current_transmission_.initiating_listener = nullptr;
    }
    this->current_transmission_.reply_pending = false;
    return;
  } else if (!this->current_transmission_.reply_pending) {
    for (auto &mv_listener : this->micronova_listeners_) {
      if (mv_listener->get_needs_update()) {
        mv_listener->set_needs_update(false);
        this->current_transmission_.initiating_listener = mv_listener;
        mv_listener->request_value_from_stove();
        return;
      }
    }
  }
}

void MicroNova::request_address(uint8_t location, uint8_t address, MicroNovaSensorListener *listener) {
  uint8_t write_data[2] = {0, 0};
  uint8_t trash_rx;

  if (this->reply_pending_mutex_.try_lock()) {
    // clear rx buffer.
    // Stove hickups may cause late replies in the rx
    while (this->available()) {
      this->read_byte(&trash_rx);
      ESP_LOGW(TAG, "Reading excess byte 0x%02X", trash_rx);
    }

    write_data[0] = location;
    write_data[1] = address;
    ESP_LOGV(TAG, "Request from stove [%02X,%02X]", write_data[0], write_data[1]);

    this->enable_rx_pin_->digital_write(true);
    this->write_array(write_data, 2);
    this->flush();
    this->enable_rx_pin_->digital_write(false);

    this->current_transmission_.request_transmission_time = millis();
    this->current_transmission_.memory_location = location;
    this->current_transmission_.memory_address = address;
    this->current_transmission_.reply_pending = true;
    this->current_transmission_.initiating_listener = listener;
  } else {
    ESP_LOGE(TAG, "Reply is pending, skipping read request");
  }
}

int MicroNova::read_stove_reply() {
  uint8_t reply_data[2] = {0, 0};
  uint8_t checksum = 0;

  // assert enable_rx_pin is false
  this->read_array(reply_data, 2);

  this->reply_pending_mutex_.unlock();
  ESP_LOGV(TAG, "Reply from stove [%02X,%02X]", reply_data[0], reply_data[1]);

  checksum = ((uint16_t) this->current_transmission_.memory_location +
              (uint16_t) this->current_transmission_.memory_address + (uint16_t) reply_data[1]) &
             0xFF;
  if (reply_data[0] != checksum) {
    ESP_LOGE(TAG, "Checksum missmatch! From [0x%02X:0x%02X] received [0x%02X,0x%02X]. Expected 0x%02X, got 0x%02X",
             this->current_transmission_.memory_location, this->current_transmission_.memory_address, reply_data[0],
             reply_data[1], checksum, reply_data[0]);
    return -1;
  }
  return ((int) reply_data[1]);
}

void MicroNova::write_address(uint8_t location, uint8_t address, uint8_t data) {
  uint8_t write_data[4] = {0, 0, 0, 0};
  uint16_t checksum = 0;

  if (this->reply_pending_mutex_.try_lock()) {
    write_data[0] = location;
    write_data[1] = address;
    write_data[2] = data;

    checksum = ((uint16_t) write_data[0] + (uint16_t) write_data[1] + (uint16_t) write_data[2]) & 0xFF;
    write_data[3] = checksum;

    ESP_LOGV(TAG, "Write 4 bytes [%02X,%02X,%02X,%02X]", write_data[0], write_data[1], write_data[2], write_data[3]);

    this->enable_rx_pin_->digital_write(true);
    this->write_array(write_data, 4);
    this->flush();
    this->enable_rx_pin_->digital_write(false);

    this->current_transmission_.request_transmission_time = millis();
    this->current_transmission_.memory_location = location;
    this->current_transmission_.memory_address = address;
    this->current_transmission_.reply_pending = true;
    this->current_transmission_.initiating_listener = nullptr;
  } else {
    ESP_LOGE(TAG, "Reply is pending, skipping write");
  }
}

}  // namespace micronova
}  // namespace esphome
