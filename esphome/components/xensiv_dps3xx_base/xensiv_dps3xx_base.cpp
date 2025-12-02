#include "esphome/core/log.h"
#include "xensiv_dps3xx_base.h"
#include <cstring>

namespace esphome {
namespace xensiv_dps3xx_base {
static const char *const TAG = "xensiv_dps3xx.component";

void XensivDPS3xx::setup() {
  this->Dps3xxPressureSensor = Dps3xx(this);
  Dps3xxPressureSensor.begin();

  // Configure interrupts if pin is provided
  if (this->interrupt_pin_ != nullptr) {
    // Setup GPIO interrupt pin first
    this->interrupt_pin_->setup();
    this->interrupt_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
    this->interrupt_pin_->attach_interrupt(XensivDPS3xx::gpio_intr, this, gpio::INTERRUPT_FALLING_EDGE);
  }
}

void XensivDPS3xx::loop() {
  // Check if data is ready via interrupt
  if (this->data_ready_) {
    this->data_ready_ = false;  // Clear flag
    // TODO: Read sensor data
    ESP_LOGW(TAG, "Data ready interrupt received - reading sensor data not yet implemented");
  }
}

void XensivDPS3xx::gpio_intr(XensivDPS3xx *arg) { arg->data_ready_ = true; }

bool XensivDPS3xx::measure_now() {
  float temperature;
  float pressure;
  uint8_t oversampling = 7;
  int16_t ret;
  Serial.println();

  /*
   * lets the Dps3xx perform a Single temperature measurement with the last (or standard) configuration
   * The result will be written to the parameter temperature
   * ret = Dps3xxPressureSensor.measureTempOnce(temperature);
   * the commented line below does exactly the same as the one above, but you can also config the precision
   * oversampling can be a value from 0 to 7
   * the Dps 3xx will perform 2^oversampling internal temperature measurements and combine them to one result with
   * higher precision measurements with higher precision take more time, consult datasheet for more information
   */
  ret = Dps3xxPressureSensor.measureTempOnce(temperature, oversampling);

  if (ret != 0) {
    /*
     * Something went wrong.
     * Look at the library code for more information about return codes
     */
    Serial.print("FAIL! ret = ");
    Serial.println(ret);
  } else {
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" degrees of Celsius");
  }

  /*
   * Pressure measurement behaves like temperature measurement
   * ret = Dps3xxPressureSensor.measurePressureOnce(pressure);
   */
  ret = Dps3xxPressureSensor.measurePressureOnce(pressure, oversampling);
  if (ret != 0) {
    // Something went wrong.
    // Look at the library code for more information about return codes
    Serial.print("FAIL! ret = ");
    Serial.println(ret);
  } else {
    Serial.print("Pressure: ");
    Serial.print(pressure);
    Serial.println(" Pascal");
  }
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
  return true;
}

}  // namespace xensiv_dps3xx_base
}  // namespace esphome
