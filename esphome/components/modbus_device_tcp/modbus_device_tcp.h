#pragma once

#if defined(USE_ESP32) && !defined(ARDUINO)

#include "esphome/core/component.h"
#include "modbus_params.h"

namespace esphome {
namespace modbus_device_tcp {

static const char *const TAG = "modbus_device_tcp";
constexpr uint32_t MB_RESPONSE_TIMEOUT_MS = 200;

class ModbusDeviceTCP : public esphome::Component {
 public:
  void set_port(uint16_t port) { this->port_ = port; }
  void set_unit_id(uint8_t id) { this->unit_id_ = id; }
  void set_num_objects(uint16_t n) { this->num_objects_ = n; }

  /// Set coil at index (0..num_objects-1). Call from interval/automation/lambda.
  void set_coil(uint16_t index, bool value) {
    if (index >= this->num_objects_)
      return;
    uint16_t byte_idx = index / 8u;
    uint8_t bit_mask = (uint8_t) (1u << (index % 8u));
    if (value)
      coil_reg_params_.coil_data[byte_idx] |= bit_mask;
    else
      coil_reg_params_.coil_data[byte_idx] &= (uint8_t) ~bit_mask;
  }

  /// Set holding register at index (0..num_objects-1). Call from interval/automation/lambda.
  void set_holding_register(uint16_t index, uint16_t value) {
    if (index >= this->num_objects_)
      return;
    holding_reg_params_.holding_regs[index] = value;
  }

  /// Set input register at index (0..num_objects-1). Call from interval/automation/lambda.
  void set_input_register(uint16_t index, uint16_t value) {
    if (index >= this->num_objects_)
      return;
    input_reg_params_.input_regs[index] = value;
  }

  /// Set discrete input at index (0..num_objects-1). Call from interval/automation/lambda.
  void set_discrete_input(uint16_t index, bool value) {
    if (index >= this->num_objects_)
      return;
    uint16_t byte_idx = index / 8u;
    uint8_t bit_mask = (uint8_t) (1u << (index % 8u));
    if (value)
      discrete_reg_params_.discrete_data[byte_idx] |= bit_mask;
    else
      discrete_reg_params_.discrete_data[byte_idx] &= (uint8_t) ~bit_mask;
  }

  void setup() override {
    // Do not start Modbus here: LwIP is not ready yet in ESPHome's setup().
    // Start in loop() once WiFi is connected (same as typical ESP-IDF order).
  }

  void dump_config() override;
  void loop() override;

 private:
  void start_modbus_();

  void *device_handler_ = nullptr;
  uint16_t port_ = 1502;
  uint8_t unit_id_ = 1;
  uint16_t num_objects_ = 5;
  bool modbus_attempted_ = false;
  bool modbus_started_ = false;

  DiscreteRegParamsT discrete_reg_params_{};
  CoilRegParamsT coil_reg_params_{};
  InputRegParamsT input_reg_params_{};
  HoldingRegParamsT holding_reg_params_{};
};
}  // namespace modbus_device_tcp
}  // namespace esphome

#endif  // defined(USE_ESP32) && !defined(ARDUINO)
