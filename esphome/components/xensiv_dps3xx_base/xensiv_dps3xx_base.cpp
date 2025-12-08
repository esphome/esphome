#include "esphome/core/log.h"
#include "xensiv_dps3xx_base.h"
#include <cstring>

namespace esphome {
namespace xensiv_dps3xx_base {
static const char *const TAG = "xensiv_dps3xx.component";

void XensivDPS3xx::setup() {
  // Configure interrupts if pin is provided
  if (this->interrupt_pin_ != nullptr) {
    // Setup GPIO interrupt pin first
    this->interrupt_pin_->setup();
    this->interrupt_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
    this->interrupt_pin_->attach_interrupt(XensivDPS3xx::gpio_intr, this, gpio::INTERRUPT_FALLING_EDGE);
  } else {
    this->failure_reason_ += "interrupt pin not configured;";
  }

  this->Dps3xxPressureSensor = new Dps3xx(this);
  Dps3xxPressureSensor->begin();
  this->set_timeout(50, [this]() {
    // Set interrupt sources with low-active polarity
    if (this->Dps3xxPressureSensor->setInterruptSources(DPS3xx_PRS_INTR, 0) != DPS__SUCCEEDED) {
      this->failure_reason_ += "Failed to set interrupt sources;";
    }
    this->Dps3xxPressureSensor->getIntStatusPrsReady();
  });
}

void XensivDPS3xx::loop() {
  // Check if data is ready via interrupt
  if (this->data_ready_) {
    this->data_ready_ = false;  // Clear flag
    ESP_LOGD(TAG, "Interrupt occurred, data ready.");

    // Read and clear the interrupt status
    int16_t int_status = Dps3xxPressureSensor->getIntStatusPrsReady();
    if (int_status == 1) {  // Returns 1 when interrupt flag is set
      float pressure = 0.0f;
      Dps3xxPressureSensor->getSingleResult(pressure);
      this->pressure_sensor_->publish_state(pressure / 1000);  // Convert to kPa
    }
  }
}

void XensivDPS3xx::gpio_intr(XensivDPS3xx *arg) { arg->data_ready_ = true; }

bool XensivDPS3xx::measure_now() {
  ESP_LOGD(TAG, "Starting measure now");
  float temperature;
  float pressure;
  uint8_t oversampling = 7;
  int16_t ret;

  /*
   * lets the Dps3xx perform a Single temperature measurement with the last (or standard) configuration
   * The result will be written to the parameter temperature
   * ret = Dps3xxPressureSensor.measureTempOnce(temperature);
   * the commented line below does exactly the same as the one above, but you can also config the precision
   * oversampling can be a value from 0 to 7
   * the Dps 3xx will perform 2^oversampling internal temperature measurements and combine them to one result with
   * higher precision measurements with higher precision take more time, consult datasheet for more information
   */
  // ret = Dps3xxPressureSensor->measureTempOnce(temperature, oversampling);

  // if (ret != 0) {
  //   // Something went wrong.
  //   // Look at the library code for more information about return codes
  //   return false;
  // } else {
  //   // this->pressure_sensor_->publish_state(temperature);
  // }

  /*
   * Pressure measurement behaves like temperature measurement
   * ret = Dps3xxPressureSensor.measurePressureOnce(pressure);
   */
  // ret = Dps3xxPressureSensor->measurePressureOnce(pressure, oversampling);
  ret = Dps3xxPressureSensor->startMeasurePressureOnce(oversampling);
  ESP_LOGD(TAG, "measurePressureOnce() returned: %d", ret);
  if (ret != 0) {
    // Something went wrong.
    // Look at the library code for more information about return codes
    return false;
  } else {
    // this->pressure_sensor_->publish_state(pressure / 1000);
  }
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

  if (this->pressure_sensor_ != nullptr) {
    LOG_SENSOR("  ", "Pressure Sensor", this->pressure_sensor_);
  }

  if (this->interrupt_pin_ != nullptr) {
    LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  } else {
    ESP_LOGCONFIG(TAG, "  Interrupt Pin: Not configured");
  }
  if (this->Dps3xxPressureSensor != nullptr) {
    ESP_LOGCONFIG(TAG, "  Dps3xxPressureSensor m_initFail: %d", this->Dps3xxPressureSensor->m_initFail);
  }
}

}  // namespace xensiv_dps3xx_base
}  // namespace esphome
