#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/esp32_ble/ble_server.h"
#include "esphome/components/esp32_ble/ble_characteristic.h"
#include "esphome/components/output/binary_output.h"
#include "esphome/components/wifi/wifi_component.h"
#include "esphome/components/improv/improv.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace esp32_improv {

class ESP32ImprovComponent : public Component, public esp32_ble::BLEServiceComponent {
 public:
  ESP32ImprovComponent();
  void dump_config() override;
  void loop() override;
  void setup_service() override;
  void setup_characteristics();
  void on_client_disconnect() override;

  float get_setup_priority() const override;
  void start() override;
  void stop() override;
  bool is_active() const { return this->state_ != improv::STATE_STOPPED; }

  void set_authorizer(binary_sensor::BinarySensor *authorizer) { this->authorizer_ = authorizer; }
  void set_status_indicator(output::BinaryOutput *status_indicator) { this->status_indicator_ = status_indicator; }
  void set_identify_duration(uint32_t identify_duration) { this->identify_duration_ = identify_duration; }
  void set_authorized_duration(uint32_t authorized_duration) { this->authorized_duration_ = authorized_duration; }

 protected:
  bool should_start_{false};
  bool setup_complete_{false};

  uint32_t identify_start_{0};
  uint32_t identify_duration_;
  uint32_t authorized_start_{0};
  uint32_t authorized_duration_;

  std::vector<uint8_t> incoming_data_;
  wifi::WiFiAP connecting_sta_;

  esp32_ble::BLEService *service_;
  esp32_ble::BLECharacteristic *status_;
  esp32_ble::BLECharacteristic *error_;
  esp32_ble::BLECharacteristic *rpc_;
  esp32_ble::BLECharacteristic *rpc_response_;
  esp32_ble::BLECharacteristic *capabilities_;

  binary_sensor::BinarySensor *authorizer_{nullptr};
  output::BinaryOutput *status_indicator_{nullptr};

  improv::State state_{improv::STATE_STOPPED};
  improv::Error error_state_{improv::ERROR_NONE};

  void set_state_(improv::State state);
  void set_error_(improv::Error error);
  void send_response(std::vector<uint8_t> &response);
  void process_incoming_data_();
  void on_wifi_connect_timeout_();
  bool check_identify_();
};

extern ESP32ImprovComponent *global_improv_component;

}  // namespace esp32_improv
}  // namespace esphome

#endif
