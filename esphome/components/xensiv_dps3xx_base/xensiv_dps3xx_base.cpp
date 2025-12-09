#include "esphome/core/log.h"
#include "xensiv_dps3xx_base.h"
#include <cstring>
#include <cmath>

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

  if (this->operation_mode_ == 1) { /* Continuous mode: use FIFO full interrupt */
    if (this->Dps3xxPressureSensor->setInterruptSources(DPS3xx_FIFO_FULL_INTR, 0) != DPS__SUCCEEDED) {
      this->failure_reason_ += "Failed to set FIFO interrupt;";
      this->mark_failed();
      return;
    }
    Dps3xxPressureSensor->getIntStatusFifoFull();

    // Use default of 1 second if not configured
    if (this->sensor_rate_value_ == 0) {
      this->sensor_rate_value_ = 1;
    }

    // Clamp sensor_rate_value to valid range [0.125s - 16s]
    // Min: 0.125s (mr=7 → 128 Hz each → 256 total → 32/256 = 0.125s)
    // Max: 16s (mr=0 → 1 Hz each → 2 total → 32/2 = 16s)
    if (this->sensor_rate_value_ < 0.125f) {
      this->sensor_rate_value_ = 0.125f;
    } else if (this->sensor_rate_value_ > 16.0f) {
      this->sensor_rate_value_ = 16.0f;
    }

    // Calculate measurement rate to fill FIFO (32 samples) in sensor_rate_value_ seconds
    // FIFO fills with both temp and pressure, so: 2 * (2^mr) * sensor_rate_value_ = 32
    // Therefore: 2^mr = 16 / sensor_rate_value_
    // mr = log2(16 / sensor_rate_value_)

    float target_rate = 16.0f / this->sensor_rate_value_;  // Measurements per second per type
    int16_t temp_mr = static_cast<int16_t>(std::round(std::log2(target_rate)));
    int16_t prs_mr = temp_mr;  // Use same rate for both

    // Clamp to valid range [0, 7] (should not be needed after rate validation, but safety)
    temp_mr = std::max(0, std::min(7, static_cast<int>(temp_mr)));
    prs_mr = std::max(0, std::min(7, static_cast<int>(prs_mr)));

    // Keep oversampling low for faster measurements
    int16_t temp_osr = 1;  // 2^1 = 2 internal measurements
    int16_t prs_osr = 1;

    ESP_LOGI(TAG, "Configured for %.1fs update interval: temp_mr=%d (%.0f Hz), prs_mr=%d (%.0f Hz)",
             this->sensor_rate_value_, temp_mr, std::pow(2.0f, temp_mr), prs_mr, std::pow(2.0f, prs_mr));

    if (this->Dps3xxPressureSensor->startMeasureBothCont(temp_mr, temp_osr, prs_mr, prs_osr) != DPS__SUCCEEDED) {
      this->failure_reason_ += "Failed to start continuous measurement;";
      this->mark_failed();
      return;
    }
    return;

  } else if (this->operation_mode_ == 0) { /* Single-shot mode: use measurement ready interrupts */
    if (this->Dps3xxPressureSensor->setInterruptSources(DPS3xx_BOTH_INTR, 0) != DPS__SUCCEEDED) {
      this->failure_reason_ += "Failed to set interrupt sources;";
      this->mark_failed();
      return;
    }
    Dps3xxPressureSensor->getIntStatusPrsReady();

  } else if (this->operation_mode_ == 2) { /* Polling Mode */
    // no interrupts
  } else { /* Invalid operation mode */
    this->failure_reason_ += "Invalid operation mode configured;";
    this->mark_failed();
    return;
  }
}

void XensivDPS3xx::loop() {
  if (data_ready_) {
    data_ready_ = false;
    ESP_LOGW(TAG, "Data ready interrupt occurred.");

    if (operation_mode_ == 1) { /* Continuous mode */
      // read all available samples from FIFO
      uint8_t pressureCount = 20;
      float pressure[pressureCount];
      uint8_t temperatureCount = 20;
      float temperature[temperatureCount];
      int16_t ret = Dps3xxPressureSensor->getContResults(temperature, temperatureCount, pressure, pressureCount);
      if (ret == DPS__SUCCEEDED) {
        this->Dps3xxPressureSensor->getIntStatusFifoFull();

        // Calculate and publish average temperature
        if (temperatureCount > 0) {
          float temp_sum = 0.0f;
          for (uint8_t i = 0; i < temperatureCount; i++) {
            temp_sum += temperature[i];
          }
          this->temperature_sensor_->publish_state(temp_sum / temperatureCount);
        }

        // Calculate and publish average pressure
        if (pressureCount > 0) {
          float prs_sum = 0.0f;
          for (uint8_t i = 0; i < pressureCount; i++) {
            prs_sum += pressure[i];
          }
          this->pressure_sensor_->publish_state(prs_sum / pressureCount / 1000.0f);  // Convert to hPa
        }
      }
    } else if (operation_mode_ == 0) {
      ESP_LOGW(TAG, "Reading data in single-shot mode.");
      // In single-shot mode, read one temperature and one pressure value
      int res = Dps3xxPressureSensor->getIntStatusPrsReady();
      if (res == 1) {
        ESP_LOGW(TAG, "getIntStatusPrsReady() returned: %d", res);
        float result = 0.0f;
        int16_t ret = Dps3xxPressureSensor->getSingleResult(result);
        if (ret != DPS__SUCCEEDED) {
          ESP_LOGW(TAG, "getSingleResult() returned: %d", ret);
          return;
        } else {
          this->pressure_sensor_->publish_state(result / 1000.0f);  // Convert to hPa
        }
        return;
      }
    }
  } else {
    // No data ready
    return;
  }
}

void XensivDPS3xx::update() {
  if (this->operation_mode_ == 2) { /* Polling Mode */
    measure_now();
  }
}

void XensivDPS3xx::gpio_intr(XensivDPS3xx *arg) { arg->data_ready_ = true; }

/**
 * @brief Perform a single pressure and a single temperature measurement
 *
 * @return true
 * @return false
 */
bool XensivDPS3xx::measure_now() {
  uint8_t osr = 7;  // oversampling rate
  // Calculate timeout using same formula as measurePressureOnce()
  uint32_t busy_time_us = 20U + (16U << osr);  // Formula from calcBusyTime with mr=0
  uint32_t timeout_ms = (busy_time_us / 10U) + 10U;

  // start pressure measurement
  if (this->Dps3xxPressureSensor->startMeasurePressureOnce(osr) != DPS__SUCCEEDED) {
    ESP_LOGW(TAG, "startMeasurePressureOnce() failed in update()");
  }
  this->set_timeout(timeout_ms, [this, osr, timeout_ms]() {
    float pressure = 0.0f;
    if (this->Dps3xxPressureSensor->getSingleResult(pressure) != DPS__SUCCEEDED) {
      ESP_LOGW(TAG, "getSingleResult() failed in update()");
    } else {
      this->pressure_sensor_->publish_state(pressure / 1000.0f);  // Convert to hPa
    }

    // start temperature measurement
    if (this->Dps3xxPressureSensor->startMeasureTempOnce(osr) != DPS__SUCCEEDED) {
      ESP_LOGW(TAG, "startMeasurePressureOnce() failed in update()");
      return;
    }
    this->set_timeout(timeout_ms, [this]() {
      float temperature = 0.0f;
      if (this->Dps3xxPressureSensor->getSingleResult(temperature) != DPS__SUCCEEDED) {
        ESP_LOGW(TAG, "getSingleResult() failed in update()");
      } else {
        this->temperature_sensor_->publish_state(temperature);
      }
    });
  });
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
  if (this->temperature_sensor_ != nullptr) {
    LOG_SENSOR("  ", "Temperature Sensor", this->temperature_sensor_);
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
