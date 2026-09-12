#pragma once
#if defined(USE_CSI_CAMERA) && defined(USE_ESP32) && defined(USE_ESP32_VARIANT_ESP32P4)

#include "esphome/components/i2c/i2c.h"

#include <cstddef>
#include <cstdint>
#include <esp_err.h>

#include <esp_sccb_io_interface.h>

namespace esphome::csi_camera {

/// Adapter connecting ESPHome I2CDevice with the ESP-IDF SCCB interface.
/// Pattern from DT-art1's camera_sensor PR — implements esp_sccb_io_t directly,
/// so no esp_sccb_new_i2c_io() factory (and no second I2C master bus) is needed.
struct I2CSCCBAdapter : esp_sccb_io_t {
  explicit I2CSCCBAdapter(i2c::I2CDevice *device) : device_(device) {
    this->transmit_reg_a8v8 = transmit;
    this->transmit_reg_a16v8 = transmit;
    this->transmit_reg_a8v16 = transmit;
    this->transmit_reg_a16v16 = transmit;
    this->transmit_receive_reg_a8v8 = transmit_receive;
    this->transmit_receive_reg_a16v8 = transmit_receive;
    this->transmit_receive_reg_a8v16 = transmit_receive;
    this->transmit_receive_reg_a16v16 = transmit_receive;
    this->transmit_v16 = transmit;
    this->receive_v16 = receive;
    this->del = del_cb;
  }
  static esp_err_t transmit(esp_sccb_io_t *io, const uint8_t *wbuf, size_t wlen, int timeout_ms) {
    (void) timeout_ms;
    return reinterpret_cast<I2CSCCBAdapter *>(io)->device_->write(wbuf, wlen) == i2c::ERROR_OK ? ESP_OK : ESP_FAIL;
  }
  static esp_err_t transmit_receive(esp_sccb_io_t *io, const uint8_t *wbuf, size_t wlen, uint8_t *rbuf, size_t rlen,
                                    int timeout_ms) {
    (void) timeout_ms;
    auto *adapter = reinterpret_cast<I2CSCCBAdapter *>(io);
    return adapter->device_->write_read(wbuf, wlen, rbuf, rlen) == i2c::ERROR_OK ? ESP_OK : ESP_FAIL;
  }
  static esp_err_t receive(esp_sccb_io_t *io, uint8_t *rbuf, size_t rlen, int timeout_ms) {
    (void) timeout_ms;
    return reinterpret_cast<I2CSCCBAdapter *>(io)->device_->read(rbuf, rlen) == i2c::ERROR_OK ? ESP_OK : ESP_FAIL;
  }
  static esp_err_t del_cb(esp_sccb_io_t *io) {
    (void) io;
    return ESP_OK;
  }
  i2c::I2CDevice *device_;
};

}  // namespace esphome::csi_camera
#endif
