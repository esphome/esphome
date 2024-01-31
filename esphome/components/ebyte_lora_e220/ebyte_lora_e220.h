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

namespace esphome {
namespace ebyte_lora_e220 {

static const char *const TAG = "ebyte_lora_e220";

class EbyteLoraE220 : public PollingComponent, public uart::UARTDevice {
 public:
  lora_e220::LoRa_E220 e220ttl = lora_e220::LoRa_E220(this, pin_aux, pin_m0, pin_m1);  //  SERIAL AUX M0 M1
  void set_message_sensor(text_sensor::TextSensor *s) { message_text_sensor = s; }
  void set_status_sensor(text_sensor::TextSensor *s) { status_text_sensor = s; }
  void set_rssi_sensor(sensor::Sensor *s) { rssi_sensor = s; }
  void set_pin_aux(GPIOPin *s) { pin_aux = s; }
  void set_pin_m0(GPIOPin *s) { pin_m0 = s; }
  void set_pin_m1(GPIOPin *s) { pin_m1 = s; }
  void setup() override {}
  void dump_config() override { ESP_LOGCONFIG(TAG, "Ebyte Lora E220"); }
  void loop() override {}
  void update() override {
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

 protected:
  std::vector<uint8_t> buffer_;
  text_sensor::TextSensor *message_text_sensor;
  text_sensor::TextSensor *status_text_sensor;
  sensor::Sensor *rssi_sensor;
  GPIOPin *pin_aux;
  GPIOPin *pin_m0;
  GPIOPin *pin_m1;
};

}  // namespace ebyte_lora_e220
}  // namespace esphome
