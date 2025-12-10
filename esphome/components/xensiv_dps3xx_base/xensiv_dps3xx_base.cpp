#include "esphome/core/log.h"
#include "xensiv_dps3xx_base.h"
#include <cstring>
#include <cmath>

namespace esphome {
namespace xensiv_dps3xx_base {
static const char *const TAG = "xensiv_dps3xx.component";

void XensivDPS3xx::setup() {
  this->Dps3xxPressureSensor = new Dps3xx(this);
  Dps3xxPressureSensor->begin();
}

void XensivDPS3xx::update() {
  if (this->operation_mode_ == 1) { /* Polling Mode */
    measure_now();
  }
}

/**
 * @brief Takes single measurement of pressure and temperature (non-blocking wait)
 *
 */
void XensivDPS3xx::measure_now() {
  uint8_t osr = 7;  // oversampling rate
  // Calculate timeout using same formula as measurePressureOnce()
  uint32_t busy_time_us = 20U + (16U << osr);  // Formula from calcBusyTime with mr=0
  uint32_t timeout_ms = (busy_time_us / 10U) + 10U;

  // start pressure measurement
  int16_t res_pressure = this->Dps3xxPressureSensor->startMeasurePressureOnce(osr);
  if (res_pressure != DPS__SUCCEEDED) {
    ESP_LOGW(TAG, "startMeasurePressureOnce() failed in update()");
  }
  this->set_timeout(timeout_ms, [this, res_pressure, osr, timeout_ms]() {
    float pressure = 0.0f;
    if (res_pressure != DPS__SUCCEEDED || this->Dps3xxPressureSensor->getSingleResult(pressure) != DPS__SUCCEEDED) {
      ESP_LOGW(TAG, "Pressure: getSingleResult() failed in update()");
    } else {
      this->pressure_sensor_->publish_state(pressure / 100.0f);  // Convert to hPa
    }

    // start temperature measurement
    if (this->Dps3xxPressureSensor->startMeasureTempOnce(osr) != DPS__SUCCEEDED) {
      ESP_LOGW(TAG, "startMeasureTempOnce() failed in update()");
      return;
    }
    this->set_timeout(timeout_ms, [this]() {
      float temperature = 0.0f;
      if (this->Dps3xxPressureSensor->getSingleResult(temperature) != DPS__SUCCEEDED) {
        ESP_LOGW(TAG, "Temperature: getSingleResult() failed in update()");
      } else {
        this->temperature_sensor_->publish_state(temperature);
      }
    });
  });
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
  if (this->temperature_sensor_ != nullptr) {
    LOG_SENSOR("  ", "Temperature Sensor", this->temperature_sensor_);
  }
  if (this->Dps3xxPressureSensor != nullptr) {
    ESP_LOGCONFIG(TAG, "  Dps3xxPressureSensor m_initFail: %d", this->Dps3xxPressureSensor->m_initFail);
  }
}

}  // namespace xensiv_dps3xx_base
}  // namespace esphome
