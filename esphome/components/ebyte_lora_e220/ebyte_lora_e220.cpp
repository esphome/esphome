#include "ebyte_lora_e220.h"

namespace esphome {
namespace ebyte_lora_e220 {
void EbyteLoraE220::update() {
  if (this->latitude_sensor_ != nullptr)
    this->latitude_sensor_->publish_state(this->latitude_);

  if (this->longitude_sensor_ != nullptr)
    this->longitude_sensor_->publish_state(this->longitude_);

  if (this->rssi_sensor != nullptr)
    this->rssi_sensor->publish_state(this->rssi_);

  // raw info
  if (this->message_text_sensor != nullptr)
    this->message_text_sensor->publish_state(this->raw_message_);
}
void EbyteLoraE220::setup() {
  this->pin_aux->setup();
  this->pin_m0->setup();
  this->pin_m1->setup();
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
  if (status)
    ESP_LOGD(TAG, "Setup success");
  else
    ESP_LOGD(TAG, "Something went wrong");
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
    ESP_LOGD(TAG, "Mode set");
    return true;
  } else {
    ESP_LOGD(TAG, "No success setting mode");
    return false;
  }
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
  return true;
}
void EbyteLoraE220::dump_config() {
  ESP_LOGCONFIG(TAG, "Ebyte Lora E220");
  LOG_PIN("  Aux pin: ", this->pin_aux);
  LOG_PIN("  M0 Pin: ", this->pin_m0);
  LOG_PIN("  M1 Pin: ", this->pin_m1);
};
void EbyteLoraE220::loop() {
  if (!available()) {
    return;
  }
  std::string buffer;
  ESP_LOGD(TAG, "Starting to read message");
  while (available()) {
    uint8_t c;
    if (this->read_byte(&c)) {
      buffer += (char) c;
    }
  }
  ESP_LOGD(TAG, "%s", buffer);
  // set the rssi
  rssi_ = atoi(buffer.substr(buffer.length() - 1, 1).c_str());
  // set the raw message
  raw_message_ = buffer.substr(0, buffer.length() - 1);
  /*
   * Starting to process the raw message
   */
  // found gps data in the raw message, lets parse it
  if (raw_message_.find("gps:") != std::string::npos) {
    int start = raw_message_.find(',');
    // minus gps
    latitude_ = atof(raw_message_.substr(4, start).c_str());
    latitude_ = atof(raw_message_.substr(start + 1, raw_message_.length()).c_str());
    ESP_LOGD(TAG, "Location:");
    ESP_LOGD(TAG, "  Lat: %f", this->latitude_);
    ESP_LOGD(TAG, "  Lon: %f", this->longitude_);
  }
}

}  // namespace ebyte_lora_e220
}  // namespace esphome
