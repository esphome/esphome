#include "lora.h"

namespace esphome {
namespace lora {
void Lora::update() {
  if (this->rssi_sensor_ != nullptr)
    this->rssi_sensor_->publish_state(this->rssi_);

  // raw info
  if (this->message_text_sensor_ != nullptr)
    this->message_text_sensor_->publish_state(this->raw_message_);
}
void Lora::setup() {
  this->pin_aux_->setup();
  this->pin_m0_->setup();
  this->pin_m1_->setup();
  if (this->pin_aux_ != nullptr) {
    this->pin_aux_->pin_mode(gpio::FLAG_INPUT);
    ESP_LOGD(TAG, "Init AUX pin!");
  }
  if (this->pin_m0_ != nullptr) {
    this->pin_m0_->pin_mode(gpio::FLAG_OUTPUT);
    ESP_LOGD(TAG, "Init M0 pin!");
    this->pin_m0_->digital_write(true);
  }
  if (this->pin_m1_ != nullptr) {
    this->pin_m1_->pin_mode(gpio::FLAG_OUTPUT);
    ESP_LOGD(TAG, "Init M1 pin!");
    this->pin_m1_->digital_write(true);
  }

  bool status = set_mode_(MODE_0_NORMAL);
  if (status) {
    ESP_LOGD(TAG, "Setup success");
  } else {
    ESP_LOGD(TAG, "Something went wrong");
  }
}
bool Lora::set_mode_(ModeType mode) {
  // data sheet claims module needs some extra time after mode setting (2ms)
  // most of my projects uses 10 ms, but 40ms is safer

  delay(40);

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

      default:
        return false;
    }
  }
  // data sheet says 2ms later control is returned, let's give just a bit more time
  // these modules can take time to activate pins
  delay(40);

  // wait until aux pin goes back low
  bool result = this->wait_complete_response_(1000);

  if (result) {
    this->mode_ = mode;
    ESP_LOGD(TAG, "Mode set");
    return true;
  } else {
    ESP_LOGD(TAG, "No success setting mode");
    return false;
  }
}
bool Lora::wait_complete_response_(uint32_t timeout, uint32_t wait_no_aux) {
  uint32_t t = millis();

  // make darn sure millis() is not about to reach max data type limit and start over
  if ((t + timeout) == 0) {
    t = 0;
  }
  ESP_LOGD(TAG, "Checking if response was complete");
  // if AUX pin was supplied and look for HIGH state
  // note you can omit using AUX if no pins are available, but you will have to use delay() to let module finish
  if (this->pin_aux_ != nullptr) {
    while (!this->pin_aux_->digital_read()) {
      if ((millis() - t) > timeout) {
        ESP_LOGD(TAG, "Timeout error!");
        return false;
      }
    }
    ESP_LOGD(TAG, "AUX HIGH!");
  } else {
    // if you can't use aux pin, use 4K7 pullup with Arduino
    // you may need to adjust this value if transmissions fail
    delay(wait_no_aux);
    ESP_LOGD(TAG, "Wait no AUX pin!");
  }

  // per data sheet control after aux goes high is 2ms so delay for at least that long)
  delay(20);
  return true;
}
void Lora::dump_config() {
  ESP_LOGCONFIG(TAG, "Ebyte Lora E220");
  LOG_PIN("Aux pin:", this->pin_aux_);
  LOG_PIN("M0 Pin:", this->pin_m0_);
  LOG_PIN("M1 Pin:", this->pin_m1_);
};
void Lora::digital_write(uint8_t pin, bool value) { this->send_pin_info_(pin, value); }
bool Lora::send_pin_info_(uint8_t pin, bool value) {
  uint8_t data[3];
  data[1] = 0xA5;   // just some bit to indicate, yo this is pin info
  data[1] = pin;    // Pin to send
  data[2] = value;  // high or low
  ESP_LOGD(TAG, "Sending message");
  ESP_LOGD(TAG, "PIN: %u ", data[1]);
  ESP_LOGD(TAG, "VALUE: %u ", data[2]);
  this->write_array(data, sizeof(data));
  bool return_value = this->wait_complete_response_(5000, 5000);
  this->flush();
  return return_value;
}
void Lora::loop() {
  std::string buffer;
  std::vector<uint8_t> data;
  bool pin_data_found = false;
  if (!this->available())
    return;
  while (this->available()) {
    uint8_t c;
    if (this->read_byte(&c)) {
      buffer += (char) c;
      // indicates that there is pin data, lets capture that
      if (c == 0xA5) {
        pin_data_found = true;
      }
      if (pin_data_found)
        data.push_back(c);
    }
  }
  ESP_LOGD(TAG, "Got %s", buffer.c_str());
  if (!data.empty()) {
    ESP_LOGD(TAG, "Found pin data!");
    ESP_LOGD(TAG, "PIN: %u ", data[1]);
    ESP_LOGD(TAG, "VALUE: %u ", data[2]);
  }
  char *ptr;
  // set the rssi
  rssi_ = strtol(buffer.substr(buffer.length() - 1, 1).c_str(), &ptr, 2);
  ESP_LOGD(TAG, "RSSI: %u ", rssi_);
  // set the raw message
  raw_message_ = buffer.substr(0, buffer.length() - 1);
}
}  // namespace lora
}  // namespace esphome
