#pragma once

#if USE_ESP32_VARIANT_ESP32P4 && USE_CSI_CAMERA_SENSOR

#include "esphome/components/i2c/i2c.h"

#include "esp_sccb_io_interface.h"

namespace esphome::camera_sensor {

/// Adapter connecting ESPHome I2CDevice with ESP-IDF SCCB interface.
struct I2CSCCBAdapter : esp_sccb_io_t {
  I2CSCCBAdapter(i2c::I2CDevice *device) {
    transmit_reg_a8v8 = transmit_;
    transmit_reg_a16v8 = transmit_;
    transmit_reg_a8v16 = transmit_;
    transmit_reg_a16v16 = transmit_;
    transmit_receive_reg_a8v8 = transmit_receive_;
    transmit_receive_reg_a16v8 = transmit_receive_;
    transmit_receive_reg_a8v16 = transmit_receive_;
    transmit_receive_reg_a16v16 = transmit_receive_;
    transmit_v16 = transmit_;
    receive_v16 = receive_;
    del = del_;
    device_ = device;
  }
  static esp_err_t transmit_(esp_sccb_io_t *io_handle, const uint8_t *write_buffer, size_t write_size,
                             int xfer_timeout_ms) {
    return reinterpret_cast<I2CSCCBAdapter *>(io_handle)->device_->write(write_buffer, write_size) == i2c::ERROR_OK
               ? ESP_OK
               : ESP_FAIL;
  }
  static esp_err_t transmit_receive_(esp_sccb_io_t *io_handle, const uint8_t *write_buffer, size_t write_size,
                                     uint8_t *read_buffer, size_t read_size, int xfer_timeout_ms) {
    return reinterpret_cast<I2CSCCBAdapter *>(io_handle)->device_->write(write_buffer, write_size) == i2c::ERROR_OK &&
                   reinterpret_cast<I2CSCCBAdapter *>(io_handle)->device_->read(read_buffer, read_size) == i2c::ERROR_OK
               ? ESP_OK
               : ESP_FAIL;
  }
  static esp_err_t receive_(esp_sccb_io_t *io_handle, uint8_t *read_buffer, size_t read_size, int xfer_timeout_ms) {
    return reinterpret_cast<I2CSCCBAdapter *>(io_handle)->device_->read(read_buffer, read_size) == i2c::ERROR_OK
               ? ESP_OK
               : ESP_FAIL;
  }
  static esp_err_t del_(esp_sccb_io_t *io_handle) { return ESP_OK; }
  i2c::I2CDevice *device_;
};

}  // namespace esphome::camera_sensor

#endif
