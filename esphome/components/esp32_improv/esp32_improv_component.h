#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"

#include "esphome/components/esp32_ble_server/ble_characteristic.h"
#include "esphome/components/esp32_ble_server/ble_server.h"
#include "esphome/components/wifi/wifi_component.h"

#ifdef USE_ESP32_IMPROV_STATE_CALLBACK
#include "esphome/core/automation.h"
#endif

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

#ifdef USE_OUTPUT
#include "esphome/components/output/binary_output.h"
#endif

#include <vector>

#ifdef USE_ESP32

#include <improv.h>

namespace esphome {
namespace esp32_improv {

using namespace esp32_ble_server;

class ESP32ImprovComponent : public Component {
 public:
  ESP32ImprovComponent();
  void dump_config() override;
  void loop() override;
  void setup() override;
  void setup_characteristics();

  float get_setup_priority() const override;
  void start();
  void stop();
  bool is_active() const { return this->state_ != improv::STATE_STOPPED; }

#ifdef USE_ESP32_IMPROV_STATE_CALLBACK
  void add_on_state_callback(std::function<void(improv::State, improv::Error)> &&callback) {
    this->state_callback_.add(std::move(callback));
  }
#endif
#ifdef USE_BINARY_SENSOR
  void set_authorizer(binary_sensor::BinarySensor *authorizer) { this->authorizer_ = authorizer; }
#endif
#ifdef USE_OUTPUT
  void set_status_indicator(output::BinaryOutput *status_indicator) { this->status_indicator_ = status_indicator; }
#endif
  void set_identify_duration(uint32_t identify_duration) { this->identify_duration_ = identify_duration; }
  void set_authorized_duration(uint32_t authorized_duration) { this->authorized_duration_ = authorized_duration; }

  void set_wifi_timeout(uint32_t wifi_timeout) { this->wifi_timeout_ = wifi_timeout; }
  uint32_t get_wifi_timeout() const { return this->wifi_timeout_; }

  improv::State get_improv_state() const { return this->state_; }
  improv::Error get_improv_error_state() const { return this->error_state_; }

 protected:
  bool should_start_{false};
  bool setup_complete_{false};

  uint32_t identify_start_{0};
  uint32_t identify_duration_;
  uint32_t authorized_start_{0};
  uint32_t authorized_duration_;

  uint32_t wifi_timeout_{};

  std::vector<uint8_t> incoming_data_;
  wifi::WiFiAP connecting_sta_;

  BLEService *service_ = nullptr;
  BLECharacteristic *status_;
  BLECharacteristic *error_;
  BLECharacteristic *rpc_;
  BLECharacteristic *rpc_response_;
  BLECharacteristic *capabilities_;

#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *authorizer_{nullptr};
#endif
#ifdef USE_OUTPUT
  output::BinaryOutput *status_indicator_{nullptr};
#endif

  improv::State state_{improv::STATE_STOPPED};
  improv::Error error_state_{improv::ERROR_NONE};
#ifdef USE_ESP32_IMPROV_STATE_CALLBACK
  CallbackManager<void(improv::State, improv::Error)> state_callback_{};
#endif

  bool status_indicator_state_{false};
  void set_status_indicator_state_(bool state);

  void set_state_(improv::State state);
  void set_error_(improv::Error error);
  void send_response_(std::vector<uint8_t> &response);
  void process_incoming_data_();
  void on_wifi_connect_timeout_();
  bool check_identify_();
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern ESP32ImprovComponent *global_improv_component;

}  // namespace esp32_improv
}  // namespace esphome

#endif
