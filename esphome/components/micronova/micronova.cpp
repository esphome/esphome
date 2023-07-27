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
  // Updating all sensors in the update() would take 600ms
  for (auto &mv_listener : this->micronova_listeners_) {
    if (mv_listener->get_needs_update()) {
      mv_listener->set_needs_update(false);
      mv_listener->read_value_from_stove();
      return;
    }
  }
}

void MicroNova::write_address(uint8_t location, uint8_t address, uint8_t data) {
  uint8_t write_data[4] = {0, 0, 0, 0};
  uint8_t reply_data[2] = {0, 0};
  int checksum = 0;

  write_data[0] = location;
  write_data[1] = address;
  write_data[2] = data;
  write_data[3] = 0x00;

  checksum = write_data[0] + write_data[1] + write_data[2];
  if (checksum >= 256) {
    write_data[3] = checksum - 256;
  } else {
    write_data[3] = checksum;
  }

  ESP_LOGD(TAG, "Write 4 bytes [%02X,%02X,%02X,%02X]", write_data[0], write_data[1], write_data[2], write_data[3]);
  this->enable_rx_pin_->digital_write(true);
  this->write_array(write_data, 4);
  this->flush();
  this->enable_rx_pin_->digital_write(false);
  // Give the stove some time to reply
  delay(STOVE_REPLY_DELAY);  // NOLINT

  this->read_byte(&reply_data[0]);
  this->read_byte(&reply_data[1]);

  this->enable_rx_pin_->digital_write(true);
  ESP_LOGD(TAG, "First 2 bytes from [%02X:%02X] [%02X,%02X]", write_data[0], write_data[1], reply_data[0],
           reply_data[1]);
}

int MicroNova::read_address(uint8_t addr, uint8_t reg) {
  uint8_t data[2] = {0, 0};

  this->enable_rx_pin_->digital_write(true);
  this->write_byte(addr);
  this->write_byte(reg);
  this->flush();
  this->enable_rx_pin_->digital_write(false);
  // Give the stove some time to reply
  delay(STOVE_REPLY_DELAY);  // NOLINT

  this->read_byte(&data[0]);
  this->read_byte(&data[1]);

  this->enable_rx_pin_->digital_write(true);
  ESP_LOGD(TAG, "First 2 bytes from [0x%02X:0x%02X] [0x%02X,0x%02X] dec: [%d,%d]", addr, reg, data[0], data[1], data[0],
           data[1]);

  if ((data[0] - data[1]) != (reg + addr)) {
    ESP_LOGE(TAG, "Checksum missmatch! From [0x%02X:0x%02X] received [0x%02X,0x%02X]. Expected %d, got %d", addr, reg,
             data[0], data[1], reg + addr, data[0] - data[1]);
    return -1;
  } else {
    return ((int) data[1]);
  }
}

}  // namespace micronova
}  // namespace esphome
