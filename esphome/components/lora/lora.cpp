#include "lora.h"

namespace esphome {
namespace lora {
void Lora::update() {
  can_send_message_();
  if (!this->update_needed_)
    return;
  if (this->rssi_sensor_ != nullptr)
    this->rssi_sensor_->publish_state(this->rssi_);

  // raw info
  if (this->message_text_sensor_ != nullptr)
    this->message_text_sensor_->publish_state(this->raw_message_);
  // reset the updater
  this->update_needed_ = false;
}
void Lora::setup() {
  this->pin_aux_->setup();
  this->pin_m0_->setup();
  this->pin_m1_->setup();
  set_mode_(MODE_0_NORMAL);
  ESP_LOGD(TAG, "Setup success");
}

ModeType Lora::get_mode_() {
  ModeType internalMode = MODE_INIT;
  if (!Lora::can_send_message_()) {
    return internalMode;
  }

  bool pin1 = this->pin_m0_->digital_read();
  bool pin2 = this->pin_m1_->digital_read();
  if (!pin1 && !pin2) {
    ESP_LOGD(TAG, "MODE NORMAL!");
    internalMode = MODE_0_NORMAL;
  }
  if (pin1 && !pin2) {
    ESP_LOGD(TAG, "MODE WOR!");
    internalMode = MODE_1_WOR_TRANSMITTER;
  }
  if (!pin1 && pin2) {
    ESP_LOGD(TAG, "MODE WOR!");
    internalMode = MODE_2_WOR_RECEIVER;
  }
  if (pin1 && pin2) {
    ESP_LOGD(TAG, "MODE Conf!");
    internalMode = MODE_3_CONFIGURATION;
  }
  if (internalMode != this->mode_) {
    ESP_LOGD(TAG, "Modes are not equal, calling the set function!! , checked: %u, expected: %u", internalMode,
             this->mode_);
    set_mode_(internalMode);
  }
  return internalMode;
}
void Lora::set_mode_(ModeType mode) {
  if (!Lora::can_send_message_()) {
    return;
  }
  if (this->pin_m0_ == nullptr && this->pin_m1_ == nullptr) {
    ESP_LOGD(TAG, "The M0 and M1 pins is not set, this mean that you are connect directly the pins as you need!");
  } else {
    switch (mode) {
      case MODE_0_NORMAL:
        // Mode 0 | normal operation
        this->pin_m0_->digital_write(false);
        this->pin_m1_->digital_write(false);
        ESP_LOGD(TAG, "MODE NORMAL!");
        break;
      case MODE_1_WOR_TRANSMITTER:
        this->pin_m0_->digital_write(true);
        this->pin_m1_->digital_write(false);
        ESP_LOGD(TAG, "MODE WOR!");
        break;
      case MODE_2_WOR_RECEIVER:
        // case MODE_2_PROGRAM:
        this->pin_m0_->digital_write(false);
        this->pin_m1_->digital_write(true);
        ESP_LOGD(TAG, "MODE RECEIVING!");
        break;
      case MODE_3_CONFIGURATION:
        // Mode 3 | Setting operation
        this->pin_m0_->digital_write(true);
        this->pin_m1_->digital_write(true);
        ESP_LOGD(TAG, "MODE SLEEP CONFIG!");
        break;
      case MODE_INIT:
        ESP_LOGD(TAG, "Don't call this!");
        break;
    }
  }
  // wait until aux pin goes back low
  this->setup_wait_response_(1000);
  this->mode_ = mode;
  ESP_LOGD(TAG, "Mode is going to be set");
}
bool Lora::can_send_message_() {
  // High means no more information is needed
  if (this->pin_aux_->digital_read()) {
    if (!this->starting_to_check_ == 0 && !this->time_out_after_ == 0) {
      this->starting_to_check_ = 0;
      this->time_out_after_ = 0;
      this->flush();
      ESP_LOGD(TAG, "Aux pin is High! Can send again!");
    }
    return true;
  } else {
    // it has taken too long to complete, error out!
    if ((millis() - this->starting_to_check_) > this->time_out_after_) {
      ESP_LOGD(TAG, "Timeout error! Resetting timers");
      this->starting_to_check_ = 0;
      this->time_out_after_ = 0;
    }
    return false;
  }
}
void Lora::setup_wait_response_(uint32_t timeout) {
  if (this->starting_to_check_ != 0 || this->time_out_after_ != 0) {
    ESP_LOGD(TAG, "Wait response already set!!  %u", timeout);
  }
  ESP_LOGD(TAG, "Setting a timer for %u", timeout);
  this->starting_to_check_ = millis();
  this->time_out_after_ = timeout;
}
void Lora::dump_config() {
  ESP_LOGCONFIG(TAG, "Ebyte Lora E220");
  LOG_PIN("Aux pin:", this->pin_aux_);
  LOG_PIN("M0 Pin:", this->pin_m0_);
  LOG_PIN("M1 Pin:", this->pin_m1_);
};
void Lora::digital_write(uint8_t pin, bool value) { this->send_pin_info_(pin, value); }
void Lora::send_pin_info_(uint8_t pin, bool value) {
  if (!Lora::can_send_message_()) {
    return;
  }
  uint8_t data[3];
  data[1] = 0xFF;   // just some bit to indicate, yo this is pin info
  data[1] = pin;    // Pin to send
  data[2] = value;  // high or low
  ESP_LOGD(TAG, "Sending message");
  ESP_LOGD(TAG, "PIN: %u ", data[1]);
  ESP_LOGD(TAG, "VALUE: %u ", data[2]);
  this->write_array(data, sizeof(data));
  this->setup_wait_response_(5000);
  ESP_LOGD(TAG, "Successfully put in queue");
}
void Lora::loop() {
  std::string buffer;
  std::vector<uint8_t> data;
  if (!this->available())
    return;
  ESP_LOGD(TAG, "Reading serial");
  while (this->available()) {
    uint8_t c;
    this->read_byte(&c);
    data.push_back(c);
  }
  if (data.size() >= 4) {
    ESP_LOGD(TAG, "Total: %u ", data.size());
    ESP_LOGD(TAG, "Start bit: %#02x ", data[0]);
    ESP_LOGD(TAG, "PIN: %u ", data[1]);
    ESP_LOGD(TAG, "VALUE: %u ", data[2]);
    ESP_LOGD(TAG, "RSSI: %#02x ", data[3]);
    if (this->rssi_sensor_ != nullptr)
      this->rssi_sensor_->publish_state(data[3]);
    if (this->message_text_sensor_ != nullptr)
      this->message_text_sensor_->publish_state("Got something");
    if (this->pcf8574_ != nullptr)
      this->pcf8574_->digital_write(data[1], data[2]);
  } else {
    ESP_LOGD(TAG, "WEIRD");
  }
}
}  // namespace lora
}  // namespace esphome
