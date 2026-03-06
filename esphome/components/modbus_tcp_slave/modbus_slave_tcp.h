#pragma once
#ifdef __cplusplus
#define _Atomic(T) T
extern "C" {
#endif

#include "mbcontroller.h"
#include "modbus_params.h"
#include "esp_modbus_slave.h"
#include "esp_modbus_common.h"

#ifdef __cplusplus
}
#endif

#include "esphome/components/wifi/wifi_component.h"

namespace modbus_slave_tcp {

static const char *const TAG = "modbus_slave_tcp";
#ifndef MODBUS_DEFAULT_PORT
#define MODBUS_DEFAULT_PORT 1502  // fallback when not built via ESPHome (define from __init__.py)
#endif
#ifndef MODBUS_DEFAULT_SLAVE_ID
#define MODBUS_DEFAULT_SLAVE_ID 1  // fallback when not built via ESPHome (define from __init__.py)
#endif
#ifndef MODBUS_DEFAULT_NUM_OBJECTS
#define MODBUS_DEFAULT_NUM_OBJECTS 5  // fallback when not built via ESPHome (define from __init__.py)
#endif
constexpr uint32_t MB_RESPONSE_TIMEOUT_MS = 200;

class ModbusSlaveTCP : public esphome::Component {
 public:
  void set_port(uint16_t port) { this->port_ = port; }
  void set_slave_id(uint8_t id) { this->slave_id_ = id; }
  void set_num_objects(uint16_t n) { this->num_objects_ = n; }

  /// Set coil at index (0..num_objects-1). Call from interval/automation/lambda.
  void set_coil(uint16_t index, bool value) {
    if (index >= this->num_objects_)
      return;
    uint16_t byte_idx = index / 8u;
    uint8_t bit_mask = (uint8_t) (1u << (index % 8u));
    if (value)
      coil_reg_params.coil_data[byte_idx] |= bit_mask;
    else
      coil_reg_params.coil_data[byte_idx] &= (uint8_t) ~bit_mask;
  }

  /// Set holding register at index (0..num_objects-1). Call from interval/automation/lambda.
  void set_holding_register(uint16_t index, uint16_t value) {
    if (index >= this->num_objects_)
      return;
    holding_reg_params.holding_regs[index] = value;
  }

  /// Set input register at index (0..num_objects-1). Call from interval/automation/lambda.
  void set_input_register(uint16_t index, uint16_t value) {
    if (index >= this->num_objects_)
      return;
    input_reg_params.input_regs[index] = value;
  }

  /// Set discrete input at index (0..num_objects-1). Call from interval/automation/lambda.
  void set_discrete_input(uint16_t index, bool value) {
    if (index >= this->num_objects_)
      return;
    uint16_t byte_idx = index / 8u;
    uint8_t bit_mask = (uint8_t) (1u << (index % 8u));
    if (value)
      discrete_reg_params.discrete_data[byte_idx] |= bit_mask;
    else
      discrete_reg_params.discrete_data[byte_idx] &= (uint8_t) ~bit_mask;
  }

  void setup() override {
    // Do not start Modbus here: LwIP is not ready yet in ESPHome's setup().
    // Start in loop() once WiFi is connected (same as typical ESP-IDF order).
  }

  void dump_config() override;
  void loop() override;

 private:
  void start_modbus_();

  void *slave_handler_ = nullptr;
  uint16_t port_ = MODBUS_DEFAULT_PORT;
  uint8_t slave_id_ = MODBUS_DEFAULT_SLAVE_ID;
  uint16_t num_objects_ = MODBUS_DEFAULT_NUM_OBJECTS;
  bool modbus_attempted_ = false;
  bool modbus_started_ = false;
};
}  // namespace modbus_slave_tcp
