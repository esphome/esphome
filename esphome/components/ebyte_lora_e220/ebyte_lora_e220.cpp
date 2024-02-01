#pragma once
#include "ebyte_lora_e220.h"

namespace esphome {
namespace ebyte_lora_e220 {

void EbyteLoraE220::setup() {
  if (this->pin_aux != nullptr) {
    this->pin_aux->pin_mode(gpio::FLAG_INPUT);
    ESP_LOGD(TAG, "Init AUX pin!");
  }
  if (this->pin_m0 != nullptr) {
    this->pin_m0->pin_mode(gpio::FLAG_OUTPUT);
    ESP_LOGD(TAG, "Init M0 pin!");
    this->pin_m0->digital_write(true);
  }
  if (this->pin_m1 != nullptr) {
    this->pin_m1->pin_mode(gpio::FLAG_OUTPUT);
    ESP_LOGD(TAG, "Init M1 pin!");
    this->pin_m1->digital_write(true);
  }

  bool status = setMode(MODE_0_NORMAL);
  ESP_LOGD(TAG, "setup success %s", status);
}
bool EbyteLoraE220::setMode(MODE_TYPE mode) {
  // data sheet claims module needs some extra time after mode setting (2ms)
  // most of my projects uses 10 ms, but 40ms is safer

  delay(40);

  if (this->pin_m0 == nullptr && this->pin_m1 == nullptr) {
    ESP_LOGD(TAG, "The M0 and M1 pins is not set, this mean that you are connect directly the pins as you need!");
  } else {
    switch (mode) {
      case MODE_0_NORMAL:
        // Mode 0 | normal operation
        this->pin_m0->digital_write(false);
        this->pin_m1->digital_write(false);
        ESP_LOGD(TAG, "MODE NORMAL!");
        break;
      case MODE_1_WOR_TRANSMITTER:
        this->pin_m0->digital_write(true);
        this->pin_m1->digital_write(false);
        ESP_LOGD(TAG, "MODE WOR!");
        break;
      case MODE_2_WOR_RECEIVER:
        //		  case MODE_2_PROGRAM:
        this->pin_m0->digital_write(false);
        this->pin_m1->digital_write(true);
        ESP_LOGD(TAG, "MODE RECEIVING!");
        break;
      case MODE_3_CONFIGURATION:
        // Mode 3 | Setting operation
        this->pin_m0->digital_write(true);
        this->pin_m1->digital_write(true);
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
  bool result = this->waitCompleteResponse(1000);

  if (result) {
    this->mode = mode;
  } else {
    ESP_LOGD(TAG, "No success");
    return false;
  }

  return true;
}
bool EbyteLoraE220::waitCompleteResponse(unsigned long timeout, unsigned int waitNoAux) {
  unsigned long t = millis();

  // make darn sure millis() is not about to reach max data type limit and start over
  if (((unsigned long) (t + timeout)) == 0) {
    t = 0;
  }

  // if AUX pin was supplied and look for HIGH state
  // note you can omit using AUX if no pins are available, but you will have to use delay() to let module finish
  if (this->pin_aux != nullptr) {
    while (this->pin_aux->digital_read() == false) {
      if ((millis() - t) > timeout) {
        ESP_LOGD(TAG, "Timeout error!");
        return false;
      }
    }
    ESP_LOGD(TAG, "AUX HIGH!");
  } else {
    // if you can't use aux pin, use 4K7 pullup with Arduino
    // you may need to adjust this value if transmissions fail
    delay(waitNoAux);
    ESP_LOGD(TAG, "Wait no AUX pin!");
  }

  // per data sheet control after aux goes high is 2ms so delay for at least that long)
  delay(20);
  ESP_LOGD(TAG, "Complete!");
  return true;
}
void EbyteLoraE220::dump_config() {
  ESP_LOGCONFIG(TAG, "Ebyte Lora E220");
  LOG_PIN("  Aux pin: ", this->pin_aux);
  LOG_PIN("  M0 Pin: ", this->pin_m0);
  LOG_PIN("  M1 Pin: ", this->pin_m1);
};
void EbyteLoraE220::update() {
  // This will be called by App.loop()

  if (this->available() > 1) {
    std::string buffer;
    uint8_t data;
    while (this->available() > 0) {
      if (this->read_byte(&data)) {
        buffer += (char) data;
      }
    }
    ESP_LOGD(TAG, "%s", buffer);
    this->message_text_sensor->publish_state(buffer.substr(0, buffer.length() - 1));
    this->rssi_sensor->publish_state(atoi(buffer.substr(buffer.length() - 1, 1).c_str()));
  }
}

}  // namespace ebyte_lora_e220
}  // namespace esphome
