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
#include "esphome/core/log.h"

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
    uint8_t bit_mask = (uint8_t)(1u << (index % 8u));
    if (value)
      coil_reg_params.coil_data[byte_idx] |= bit_mask;
    else
      coil_reg_params.coil_data[byte_idx] &= (uint8_t)~bit_mask;
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
    uint8_t bit_mask = (uint8_t)(1u << (index % 8u));
    if (value)
      discrete_reg_params.discrete_data[byte_idx] |= bit_mask;
    else
      discrete_reg_params.discrete_data[byte_idx] &= (uint8_t)~bit_mask;
  }

  void setup() override {
    // Do not start Modbus here: LwIP is not ready yet in ESPHome's setup().
    // Start in loop() once WiFi is connected (same as typical ESP-IDF order).
  }

  void dump_config() override {
    ESP_LOGCONFIG(TAG, "Modbus TCP Slave: port %u, slave_id %u, num_objects %u",
                  (unsigned) this->port_, (unsigned) this->slave_id_, (unsigned) this->num_objects_);
  }

  void loop() override {
    if (this->modbus_attempted_)
      return;
    if (!esphome::wifi::global_wifi_component || !esphome::wifi::global_wifi_component->is_connected())
      return;
    this->start_modbus_();
  }

 private:
  void start_modbus_() {
    this->modbus_attempted_ = true;  // only try once; avoid retries that leak contexts and hit EADDRINUSE

    ESP_LOGI(TAG, "Starting Modbus TCP Slave, port %u, slave_id %u",
             (unsigned) this->port_, (unsigned) this->slave_id_);

    mb_communication_info_t config = {};
    config.tcp_opts.mode = MB_TCP;
    config.tcp_opts.port = this->port_;
    config.tcp_opts.uid = this->slave_id_;
    config.tcp_opts.response_tout_ms = MB_RESPONSE_TIMEOUT_MS;
    config.tcp_opts.test_tout_us = 0;
    config.tcp_opts.addr_type = MB_IPV4;
    config.tcp_opts.ip_addr_table = nullptr;
    config.tcp_opts.ip_netif_ptr = nullptr;
    config.tcp_opts.dns_name = nullptr;
    config.tcp_opts.start_disconnected = false;

    esp_err_t err = mbc_slave_create_tcp(&config, &this->slave_handler_);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "mbc_slave_create_tcp failed: %s", esp_err_to_name(err));
      return;
    }

    mb_register_area_descriptor_t reg_area;
    uint16_t coil_bytes = (this->num_objects_ + 7u) / 8u;
    uint16_t reg_bytes = this->num_objects_ * 2u;

    reg_area.type = MB_PARAM_HOLDING;
    reg_area.start_offset = 0;
    reg_area.access = MB_ACCESS_RW;
    reg_area.address = (void *)&holding_reg_params;
    reg_area.size = reg_bytes;
    mbc_slave_set_descriptor(this->slave_handler_, reg_area);

    reg_area.type = MB_PARAM_INPUT;
    reg_area.start_offset = 0;
    reg_area.access = MB_ACCESS_RO;
    reg_area.address = (void *)&input_reg_params;
    reg_area.size = reg_bytes;
    mbc_slave_set_descriptor(this->slave_handler_, reg_area);

    reg_area.type = MB_PARAM_COIL;
    reg_area.start_offset = 0;
    reg_area.access = MB_ACCESS_RW;
    reg_area.address = (void *)&coil_reg_params;
    reg_area.size = coil_bytes;
    mbc_slave_set_descriptor(this->slave_handler_, reg_area);

    reg_area.type = MB_PARAM_DISCRETE;
    reg_area.start_offset = 0;
    reg_area.access = MB_ACCESS_RO;
    reg_area.address = (void *)&discrete_reg_params;
    reg_area.size = coil_bytes;
    mbc_slave_set_descriptor(this->slave_handler_, reg_area);

    err = mbc_slave_start(this->slave_handler_);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "mbc_slave_start failed: %s", esp_err_to_name(err));
      (void)mbc_slave_delete(this->slave_handler_);
      this->slave_handler_ = nullptr;
      return;
    }
    this->modbus_started_ = true;
    ESP_LOGI(TAG, "Modbus TCP Slave listening on port %u, slave_id %u",
             (unsigned) this->port_, (unsigned) this->slave_id_);
  }

  void *slave_handler_ = nullptr;
  uint16_t port_ = MODBUS_DEFAULT_PORT;
  uint8_t slave_id_ = MODBUS_DEFAULT_SLAVE_ID;
  uint16_t num_objects_ = MODBUS_DEFAULT_NUM_OBJECTS;
  bool modbus_attempted_ = false;
  bool modbus_started_ = false;
};
}  // namespace modbus_slave_tcp
