#pragma once
#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include <HardwareSerial.h>
#include "esphome/core/helpers.h"
#include "lora_e220.h"

namespace esphome {
namespace ebyte_lora_e220 {

// there are 3 UART ports, we are going to be using 0, which is D6 and D7
HardwareSerial LoraSerial(0);
LoRa_E220 e220ttl(&LoraSerial, D2, D0, D1);  //  SERIAL AUX M0 M1

class LoRaSensors : public text_sensor::TextSensor, public PollingComponent {
 public:
  LoRaSensors() : PollingComponent(4000) {}

  void setup() override { e220ttl.begin(); }

  void update() override {
    // This will be called by App.loop()

    if (e220ttl.available() > 1) {
      // read the String message
      ResponseContainer rc = e220ttl.receiveMessageRSSI();
      // Is something goes wrong print error
      if (rc.status.code != 1) {
        this->publish_state(rc.status.getResponseDescription());
      } else {
        // Print the data received
        this->publish_state(rc.status.getResponseDescription());
        this->publish_state(rc.data);
        this->publish_state(rc.rssi + "");
      }
    }
  }
};

}  // namespace ebyte_lora_e220
}  // namespace esphome
