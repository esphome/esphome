#include "modbus_device_tcp.h"
#include "esp_err.h"
#include "esphome/core/log.h"

namespace esphome {
namespace modbus_device_tcp {

void ModbusDeviceTCP::dump_config() {
  ESP_LOGCONFIG(TAG, "Modbus TCP device: port %u, unit_id %u, num_objects %u", (unsigned) port_, (unsigned) unit_id_,
                (unsigned) num_objects_);
}

void ModbusDeviceTCP::loop() {
  if (modbus_attempted_)
    return;
  if (!esphome::wifi::global_wifi_component || !esphome::wifi::global_wifi_component->is_connected())
    return;
  start_modbus_();
}

void ModbusDeviceTCP::start_modbus_() {
  modbus_attempted_ = true;  // only try once; avoid retries that leak contexts and hit EADDRINUSE

  ESP_LOGI(TAG, "Starting Modbus TCP device, port %u, unit_id %u", (unsigned) port_, (unsigned) unit_id_);

  mb_communication_info_t config = {};
  config.tcp_opts.mode = MB_TCP;
  config.tcp_opts.port = port_;
  config.tcp_opts.uid = unit_id_;
  config.tcp_opts.response_tout_ms = MB_RESPONSE_TIMEOUT_MS;
  config.tcp_opts.test_tout_us = 0;
  config.tcp_opts.addr_type = MB_IPV4;
  config.tcp_opts.ip_addr_table = nullptr;
  config.tcp_opts.ip_netif_ptr = nullptr;
  config.tcp_opts.dns_name = nullptr;
  config.tcp_opts.start_disconnected = false;

  esp_err_t err = mbc_slave_create_tcp(&config, &device_handler_);  // NOLINT(readability-terms)
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "mbc_slave_create_tcp failed: %s", esp_err_to_name(err));  // NOLINT(readability-terms)
    return;
  }

  mb_register_area_descriptor_t reg_area;
  uint16_t coil_bytes = (num_objects_ + 7u) / 8u;
  uint16_t reg_bytes = num_objects_ * 2u;

  reg_area.type = MB_PARAM_HOLDING;
  reg_area.start_offset = 0;
  reg_area.access = MB_ACCESS_RW;
  reg_area.address = (void *) &holding_reg_params;
  reg_area.size = reg_bytes;
  mbc_slave_set_descriptor(device_handler_, reg_area);  // NOLINT(readability-terms)

  reg_area.type = MB_PARAM_INPUT;
  reg_area.start_offset = 0;
  reg_area.access = MB_ACCESS_RO;
  reg_area.address = (void *) &input_reg_params;
  reg_area.size = reg_bytes;
  mbc_slave_set_descriptor(device_handler_, reg_area);  // NOLINT(readability-terms)

  reg_area.type = MB_PARAM_COIL;
  reg_area.start_offset = 0;
  reg_area.access = MB_ACCESS_RW;
  reg_area.address = (void *) &coil_reg_params;
  reg_area.size = coil_bytes;
  mbc_slave_set_descriptor(device_handler_, reg_area);  // NOLINT(readability-terms)

  reg_area.type = MB_PARAM_DISCRETE;
  reg_area.start_offset = 0;
  reg_area.access = MB_ACCESS_RO;
  reg_area.address = (void *) &discrete_reg_params;
  reg_area.size = coil_bytes;
  mbc_slave_set_descriptor(device_handler_, reg_area);  // NOLINT(readability-terms)

  err = mbc_slave_start(device_handler_);  // NOLINT(readability-terms)
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "mbc_slave_start failed: %s", esp_err_to_name(err));  // NOLINT(readability-terms)
    (void) mbc_slave_delete(device_handler_);  // NOLINT(readability-terms)
    device_handler_ = nullptr;
    return;
  }
  modbus_started_ = true;
  ESP_LOGI(TAG, "Modbus TCP device listening on port %u, unit_id %u", (unsigned) port_, (unsigned) unit_id_);
}

}  // namespace modbus_device_tcp
}  // namespace esphome
