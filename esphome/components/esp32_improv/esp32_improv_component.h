#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/esp32_ble_server/esp32_ble_server.h"
#include "esphome/components/output/binary_output.h"
#include "esphome/components/wifi/wifi_component.h"
#include "esphome/components/improv/improv.h"

#ifdef ARDUINO_ARCH_ESP32

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

namespace esphome {
namespace esp32_improv {

class ESP32ImprovComponent : public Component, public BLECharacteristicCallbacks {
 public:
  void setup() override;
  void dump_config() override;
  void loop() override;

  float get_setup_priority() const override;
  void start();
  void end();
  bool is_active() const { return this->state_ == improv::STATE_ACTIVATED; }

  void set_activator(binary_sensor::BinarySensor *activator) { this->activator_ = activator; }
  void set_status_indicator(output::BinaryOutput *status_indicator) { this->status_indicator_ = status_indicator; }
  void set_identify_duration(uint32_t identify_duration) { this->identify_duration_ = identify_duration; }
  void set_activated_duration(uint32_t activated_duration) { this->activated_duration_ = activated_duration; }

  void onWrite(BLECharacteristic *characteristic) override;

 protected:
  uint32_t identify_start_{0};
  uint32_t identify_duration_;
  uint32_t activated_start_{0};
  uint32_t activated_duration_;

  std::string incoming_data_;
  wifi::WiFiAP connecting_sta_;

  BLEService *service_;
  BLECharacteristic *status_;
  BLECharacteristic *error_;
  BLECharacteristic *rpc_;
  BLECharacteristic *rpc_response_;

  binary_sensor::BinarySensor *activator_{nullptr};
  output::BinaryOutput *status_indicator_{nullptr};

  improv::State state_{improv::STATE_STOPPED};
  improv::Error error_state_{improv::ERROR_NONE};

  void set_state_(improv::State state);
  void set_error_(improv::Error error);
  void process_incoming_data_();
  void on_wifi_connect_timeout_();
  bool check_identify_();
};

extern ESP32ImprovComponent *global_improv_component;

}  // namespace esp32_improv
}  // namespace esphome

#endif
