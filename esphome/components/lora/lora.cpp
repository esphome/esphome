#include "lora.h"

namespace esphome {
namespace lora {
void Lora::update() {
  if (this->rssi_sensor != nullptr)
    this->rssi_sensor->publish_state(this->rssi_);

  // raw info
  if (this->message_text_sensor != nullptr)
    this->message_text_sensor->publish_state(this->raw_message_);
}
void Lora::setup() {
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
bool Lora::setMode(MODE_TYPE mode) {
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
bool Lora::waitCompleteResponse(unsigned long timeout, unsigned int waitNoAux) {
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
void Lora::dump_config() {
  ESP_LOGCONFIG(TAG, "Ebyte Lora E220");
  LOG_PIN("Aux pin:", this->pin_aux);
  LOG_PIN("M0 Pin:", this->pin_m0);
  LOG_PIN("M1 Pin:", this->pin_m1);
};
void Lora::digital_write(uint8_t pin, bool value) {
  ESP_LOGD(TAG, "Starting to write message");
  std::string message = str_snprintf("%02x%02x", 9, pin, value);
  sendMessage(message);
}
bool Lora::sendMessage(std::string message) {
  uint8_t size = message.length();
  char messageFixed[size];
  memcpy(messageFixed, message.c_str(), size);
  if (size > MAX_SIZE_TX_PACKET + 2) {
    ESP_LOGD(TAG, "Package to big");
    return false;
  }
  ESP_LOGD(TAG, "Sending: %s", message);
  this->write_array((uint8_t *) &messageFixed, size);
  bool result = this->waitCompleteResponse(5000, 5000);
  return result;
}
void Lora::loop() {
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
  ESP_LOGD(TAG, "Got %s", buffer);
  // set the rssi
  rssi_ = atoi(buffer.substr(buffer.length() - 1, 1).c_str());
  // set the raw message
  raw_message_ = buffer.substr(0, buffer.length() - 1);
}

// pin stuff

static const char *const TAGPin = "lora.pin";
void LoraGPIOPin::setup() { pin_mode(flags_); }
void LoraGPIOPin::pin_mode(gpio::Flags flags) {
  if (flags != gpio::FLAG_OUTPUT) {
    ESP_LOGD(TAGPin, "Output only supported");
  }
}
bool LoraGPIOPin::digital_read() { return false; }
void LoraGPIOPin::digital_write(bool value) { this->parent_->digital_write(this->pin_, value != this->inverted_); }
std::string LoraGPIOPin::dump_summary() const {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%u via Lora", pin_);
  return buffer;
}
}  // namespace lora
}  // namespace esphome
