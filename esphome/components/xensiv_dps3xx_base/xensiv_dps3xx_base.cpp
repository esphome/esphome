#include "esphome/core/log.h"
#include "xensiv_dps3xx_base.h"
#include <cstring>

namespace esphome {
namespace xensiv_dps3xx_base {
static const char *const TAG = "xensiv_dps3xx.component";

// Static I2C read wrapper
cy_rslt_t XensivDPS3xx::i2c_read_wrapper(void *context, uint16_t timeout, uint8_t i2c_addr, uint8_t reg_adr,
                                         uint8_t *data, uint8_t length) {
  XensivDPS3xx *self = static_cast<XensivDPS3xx *>(context);
  if (self->read_bytes(reg_adr, data, length)) {
    return CY_RSLT_SUCCESS;
  }
  return CY_RSLT_CREATE(CY_RSLT_TYPE_ERROR, CY_RSLT_MODULE_BOARD_HARDWARE_DPS3XX, 1);
}

// Static I2C write wrapper
cy_rslt_t XensivDPS3xx::i2c_write_wrapper(void *context, uint16_t timeout, uint8_t i2c_addr, uint8_t reg_adr,
                                          uint8_t *data, uint8_t length) {
  XensivDPS3xx *self = static_cast<XensivDPS3xx *>(context);
  if (length == 1) {
    if (self->write_byte(reg_adr, data[0])) {
      return CY_RSLT_SUCCESS;
    }
  } else {
    // For multi-byte writes, write each byte individually
    for (uint8_t i = 0; i < length; i++) {
      if (!self->write_byte(reg_adr + i, data[i])) {
        return CY_RSLT_CREATE(CY_RSLT_TYPE_ERROR, CY_RSLT_MODULE_BOARD_HARDWARE_DPS3XX, 2);
      }
    }
    return CY_RSLT_SUCCESS;
  }
  return CY_RSLT_CREATE(CY_RSLT_TYPE_ERROR, CY_RSLT_MODULE_BOARD_HARDWARE_DPS3XX, 2);
}

// Static delay wrapper
cy_rslt_t XensivDPS3xx::delay_wrapper(uint32_t ms) {
  delay(ms);
  return CY_RSLT_SUCCESS;
}

void XensivDPS3xx::setup() {
  // Setup I2C communication functions
  xensiv_dps3xx_i2c_comm_t comm = {.read = XensivDPS3xx::i2c_read_wrapper,
                                   .write = XensivDPS3xx::i2c_write_wrapper,
                                   .delay = XensivDPS3xx::delay_wrapper,
                                   .context = this};

  // Initialize the DPS3xx sensor
  cy_rslt_t result = xensiv_dps3xx_init_i2c(&this->dps_obj_, &comm, this->i2c_addr_);

  if (result != CY_RSLT_SUCCESS) {
    this->failure_reason_ += "Failed to initialize DPS3xx sensor;";
    this->mark_failed();
    return;
  }
}

void XensivDPS3xx::loop() {
  // Check if data is ready via interrupt
  if (this->data_ready_) {
    this->data_ready_ = false;  // Clear flag
    // TODO: Read sensor data
  }
}

void XensivDPS3xx::gpio_intr(XensivDPS3xx *arg) { arg->data_ready_ = true; }

bool XensivDPS3xx::test_scratch_register_() {
  // TODO: Implement scratch register test based on DPS3xx datasheet
  ESP_LOGD(TAG, "Scratch register test - to be implemented");
  return true;
}

bool XensivDPS3xx::measure_now() {
  // TODO: Implement measurement trigger
  ESP_LOGD(TAG, "Starting measurement");
  return true;
}

void XensivDPS3xx::dump_config() {
  ESP_LOGCONFIG(TAG, "XENSIV DPS3xx Pressure Sensor:");

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with DPS3xx failed!");
  }
  if (!this->failure_reason_.empty()) {
    ESP_LOGW(TAG, "Failure reason(s): %s", this->failure_reason_.c_str());
  }

  if (this->dps_sensor_ != nullptr) {
    LOG_SENSOR("  ", "Pressure Sensor", this->dps_sensor_);
  }

  if (this->interrupt_pin_ != nullptr) {
    LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  } else {
    ESP_LOGCONFIG(TAG, "  Interrupt Pin: Not configured");
  }
}

}  // namespace xensiv_dps3xx_base
}  // namespace esphome
