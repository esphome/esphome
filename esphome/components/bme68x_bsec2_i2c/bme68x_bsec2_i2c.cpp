#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_BSEC2
#include "bme68x_bsec2_i2c.h"
#include "esphome/components/i2c/i2c.h"

#include <cinttypes>

namespace esphome {
namespace bme68x_bsec2_i2c {

static const char *const TAG = "bme68x_bsec2_i2c.sensor";

void BME68xBSEC2I2CComponent::setup() {
  // must set up our bme68x_dev instance before calling setup()
  this->bme68x_.intf_ptr = (void *) this;
  this->bme68x_.intf = BME68X_I2C_INTF;
  this->bme68x_.read = BME68xBSEC2I2CComponent::read_bytes_wrapper;
  this->bme68x_.write = BME68xBSEC2I2CComponent::write_bytes_wrapper;
  this->bme68x_.delay_us = BME68xBSEC2I2CComponent::delay_us;
  this->bme68x_.amb_temp = 25;

  BME68xBSEC2Component::setup();
}

void BME68xBSEC2I2CComponent::dump_config() {
  LOG_I2C_DEVICE(this);
  BME68xBSEC2Component::dump_config();
}

uint32_t BME68xBSEC2I2CComponent::get_hash() { return fnv1_hash("bme68x_bsec_state_" + to_string(this->address_)); }

int8_t BME68xBSEC2I2CComponent::read_bytes_wrapper(uint8_t a_register, uint8_t *data, uint32_t len, void *intfPtr) {
  ESP_LOGVV(TAG, "read_bytes_wrapper: reg = %u", a_register);
  return static_cast<BME68xBSEC2I2CComponent *>(intfPtr)->read_bytes(a_register, data, len) ? 0 : -1;
}

int8_t BME68xBSEC2I2CComponent::write_bytes_wrapper(uint8_t a_register, const uint8_t *data, uint32_t len,
                                                    void *intfPtr) {
  ESP_LOGVV(TAG, "write_bytes_wrapper: reg = %u", a_register);
  return static_cast<BME68xBSEC2I2CComponent *>(intfPtr)->write_bytes(a_register, data, len) ? 0 : -1;
}

void BME68xBSEC2I2CComponent::delay_us(uint32_t period, void *intfPtr) {
  ESP_LOGVV(TAG, "Delaying for %" PRIu32 "us", period);
  delayMicroseconds(period);
}

}  // namespace bme68x_bsec2_i2c
}  // namespace esphome
#endif
