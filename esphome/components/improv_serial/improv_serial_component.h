#pragma once

#include "esphome/components/improv_base/improv_base.h"
#include "esphome/components/wifi/wifi_component.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#include <improv.h>
#include <vector>

#ifdef USE_ARDUINO
#include <HardwareSerial.h>
#endif
#ifdef USE_ESP_IDF
#include <driver/uart.h>
#endif

namespace esphome {
namespace improv_serial {

enum ImprovSerialType : uint8_t {
  TYPE_CURRENT_STATE = 0x01,
  TYPE_ERROR_STATE = 0x02,
  TYPE_RPC = 0x03,
  TYPE_RPC_RESPONSE = 0x04
};

static const uint8_t IMPROV_SERIAL_VERSION = 1;

class ImprovSerialComponent : public Component, public improv_base::ImprovBase {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
  bool parse_improv_serial_byte_(uint8_t byte);
  bool parse_improv_payload_(improv::ImprovCommand &command);

  void set_state_(improv::State state);
  void set_error_(improv::Error error);
  void send_response_(std::vector<uint8_t> &response);
  void on_wifi_connect_timeout_();

  std::vector<uint8_t> build_rpc_settings_response_(improv::Command command);
  std::vector<uint8_t> build_version_info_();

  int available_();
  uint8_t read_byte_();
  void write_data_(std::vector<uint8_t> &data);

#ifdef USE_ARDUINO
  Stream *hw_serial_{nullptr};
#endif
#ifdef USE_ESP_IDF
  uart_port_t uart_num_;
#endif

  std::vector<uint8_t> rx_buffer_;
  uint32_t last_read_byte_{0};
  wifi::WiFiAP connecting_sta_;
  improv::State state_{improv::STATE_AUTHORIZED};
};

extern ImprovSerialComponent
    *global_improv_serial_component;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace improv_serial
}  // namespace esphome
