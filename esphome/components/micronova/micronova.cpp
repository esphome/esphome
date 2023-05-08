#include "micronova.h"
#include "esphome/core/log.h"

namespace esphome {
namespace micronova {

static const std::string STOVE_STATES[11] = {"Off",
                                             "Start",
                                             "Pellets loading",
                                             "Igniton",
                                             "Working",
                                             "Brazier Cleaning",
                                             "Final Cleaning",
                                             "Stanby",
                                             "No pellets alarm",
                                             "No ignition alarm",
                                             "Undefined alarm"};

///////////////////////////////////////////////////////////////////////////////
// MicroNovaSensor members
void MicroNovaSensor::read_value_from_stove() {
  int val = -1;

  val = this->micronova_->read_address(this->memory_location_, this->memory_address_);

  if (val == -1) {
    this->publish_state(NAN);
    return;
  }

  this->current_data_ = (float) val;
  switch (this->get_function()) {
    case MicroNovaFunctions::STOVE_FUNCTION_ROOM_TEMPERATURE:
      this->current_data_ = (float) this->current_data_ / 2;
      break;
    case MicroNovaFunctions::STOVE_FUNCTION_THERMOSTAT_TEMPERATURE:
      this->micronova_->set_thermostat_temperature(val);
      break;
    case MicroNovaFunctions::STOVE_FUNCTION_FAN_SPEED:
      this->current_data_ = this->current_data_ == 0 ? 0 : (this->current_data_ * 10) + this->fan_speed_offset_;
      break;
    default:
      break;
  }
  this->publish_state(this->current_data_);
}

///////////////////////////////////////////////////////////////////////////////
// MicroNovaTextSensor members
void MicroNovaTextSensor::read_value_from_stove() {
  int val = -1;

  val = this->micronova_->read_address(this->memory_location_, this->memory_address_);

  if (val == -1) {
    this->publish_state("unknown");
    return;
  }

  switch (this->get_function()) {
    case MicroNovaFunctions::STOVE_FUNCTION_STOVE_STATE:
      this->micronova_->set_current_stove_state(val);
      this->publish_state(STOVE_STATES[val]);
      // set the stove switch to on for any value but 0
      if (val != 0 && this->micronova_->get_stove_switch() != nullptr && !this->micronova_->get_stove_switch()->state) {
        this->micronova_->get_stove_switch()->publish_state(true);
      } else if (val == 0 && this->micronova_->get_stove_switch() != nullptr &&
                 this->micronova_->get_stove_switch()->state) {
        this->micronova_->get_stove_switch()->publish_state(false);
      }
      break;
    default:
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////
// MicroNovaButton members
void MicroNovaButton::press_action() {
  uint8_t new_temp = 20;

  switch (this->get_function()) {
    case MicroNovaFunctions::STOVE_FUNCTION_TEMP_UP:
    case MicroNovaFunctions::STOVE_FUNCTION_TEMP_DOWN:
      new_temp = this->micronova_->get_thermostat_temperature() +
                 (MicroNovaFunctions::STOVE_FUNCTION_TEMP_UP == this->get_function() ? 1 : -1);
      this->micronova_->write_address(this->memory_location_, this->memory_address_, new_temp);
      this->micronova_->update();
      break;

    default:
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////
// MicroNovaSwitch members
void MicroNovaSwitch::write_state(bool state) {
  switch (this->get_function()) {
    case MicroNovaFunctions::STOVE_FUNCTION_SWITCH:
      if (state) {
        // Only send poweron when current state is Off
        if (micronova_->get_current_stove_state() == 0) {
          this->micronova_->write_address(this->memory_location_, this->memory_address_, this->memory_data_on_);
          this->publish_state(true);
        } else
          ESP_LOGW(TAG, "Unable to turn stove on, invalid state: %d", micronova_->get_current_stove_state());
      } else {
        // don't shut send power-off when statis is Off or Final cleaning
        if (micronova_->get_current_stove_state() != 0 && micronova_->get_current_stove_state() != 6) {
          this->micronova_->write_address(this->memory_location_, this->memory_address_, this->memory_data_off_);
          this->publish_state(false);
        } else
          ESP_LOGW(TAG, "Unable to turn stove off, invalid state: %d", micronova_->get_current_stove_state());
      }
      this->micronova_->update();
      break;

    default:
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////
// MicroNova members
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

  for (auto &mv_sensor : this->micronova_sensors_) {
    mv_sensor->dump_config();
    ESP_LOGCONFIG(TAG, "    sensor location:%02X, address:%02X", mv_sensor->get_memory_location(),
                  mv_sensor->get_memory_address());
  }

  if (this->scan_memory_location_ >= 0) {
    ESP_LOGCONFIG(TAG, "  Memory %02X scan", this->scan_memory_location_);
    for (uint8_t i = 0; i < 0x0F; i++) {
      ESP_LOGCONFIG(TAG, "    Address %02X, Data %02X", i,
                    this->read_address((uint8_t) this->scan_memory_location_, i));
    }
  }
}

void MicroNova::update() {
  for (auto &mv_sensor : this->micronova_sensors_) {
    mv_sensor->read_value_from_stove();
  }
}

void MicroNova::write_address(uint8_t location, uint8_t address, uint8_t data) {
  uint8_t write_data[4] = {0, 0, 0, 0};
  uint8_t reply_data[2] = {0, 0};
  uint8_t c;
  int i = 0, checksum = 0;

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

  while (this->available()) {
    this->read_byte(&c);
    if (i < 2) {
      reply_data[i] = c;
    } else {
      ESP_LOGW(TAG, "Received extra byte (%d), data %02X", i, c);
    }
    i++;
  }
  this->enable_rx_pin_->digital_write(true);
  ESP_LOGD(TAG, "First 2 bytes from %02X:%02X [%02X,%02X]", write_data[0], write_data[1], reply_data[0], reply_data[1]);
}

int MicroNova::read_address(uint8_t addr, uint8_t reg) {
  int i = 0;
  uint8_t c, data[2] = {0, 0};

  this->enable_rx_pin_->digital_write(true);
  this->write_byte(addr);
  this->write_byte(reg);
  this->flush();
  this->enable_rx_pin_->digital_write(false);
  // Give the stove some time to reply
  delay(STOVE_REPLY_DELAY);  // NOLINT

  while (this->available()) {
    this->read_byte(&c);
    if (i < 2) {
      data[i] = c;
    } else {
      ESP_LOGW(TAG, "Received extra byte (%d), data 0x%02X", i, c);
    }
    i++;
  }
  this->enable_rx_pin_->digital_write(true);
  ESP_LOGD(TAG, "First 2 bytes from 0x%02X:0x%02X [0x%02X,0x%02X] dec: [%d,%d]", addr, reg, data[0], data[1], data[0],
           data[1]);

  return (i == 2 ? (int) data[1] : -1);  // 2 reply bytes, no more, no less, checksum is TODO
}

}  // namespace micronova
}  // namespace esphome
