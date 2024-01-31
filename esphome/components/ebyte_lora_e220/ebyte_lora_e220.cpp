#pragma once
#include <utility>
#include <vector>
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/log.h"
#include "lora_e220.h"
#include "ebyte_lora_e220.h"

namespace esphome {
namespace ebyte_lora_e220 {

static const char *const TAG = "ebyte_lora_e220";

void EbyteLoraE220::dump_config() {
  ESP_LOGCONFIG(TAG, "Ebyte Lora E220");
  LOG_PIN("  Aux pin: ", this->pin_aux);
  LOG_PIN("  M0 Pin: ", this->pin_m0);
  LOG_PIN("  M1 Pin: ", this->pin_m1);
};
void EbyteLoraE220::update() {
  // This will be called by App.loop()

  if (e220ttl.available() > 1) {
    // read the String message
    lora_e220::ResponseContainer rc = e220ttl.receiveMessageRSSI();
    // Is something goes wrong print error
    if (rc.status.code != 1) {
      this->status_text_sensor->publish_state(rc.status.getResponseDescription());
    } else {
      // Print the data received
      this->status_text_sensor->publish_state(rc.status.getResponseDescription());
      this->message_text_sensor->publish_state(rc.data);
      this->rssi_sensor->publish_state(rc.rssi);
    }
  }
}

}  // namespace ebyte_lora_e220
}  // namespace esphome
