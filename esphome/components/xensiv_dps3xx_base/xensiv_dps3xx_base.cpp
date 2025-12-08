#include "esphome/core/log.h"
#include "xensiv_dps3xx_base.h"
#include <cstring>

namespace esphome {
namespace xensiv_dps3xx_base {
static const char *const TAG = "xensiv_dps3xx.component";

void XensivDPS3xx::setup() {
  this->Dps3xxPressureSensor = new Dps3xx(this);
  Dps3xxPressureSensor->begin();
  this->set_timeout(50, [this]() {
    if (this->operation_mode_ == 1) {
      // Continuous mode: use FIFO full interrupt
      if (this->Dps3xxPressureSensor->setInterruptSources(DPS3xx_FIFO_FULL_INTR, 0) != DPS__SUCCEEDED) {
        this->failure_reason_ += "Failed to set FIFO interrupt;";
        this->mark_failed();

        // continuous mode
        /*
         * temperature measure rate (value from 0 to 7)
         * 2^temp_mr temperature measurement results per second
         */
        int16_t temp_mr = 5;

        /*
         * temperature oversampling rate (value from 0 to 7)
         * 2^temp_osr internal temperature measurements per result
         * A higher value increases precision
         */
        int16_t temp_osr = 1;

        /*
         * pressure measure rate (value from 0 to 7)
         * 2^prs_mr pressure measurement results per second
         */
        int16_t prs_mr = 5;

        /*
         * pressure oversampling rate (value from 0 to 7)
         * 2^prs_osr internal pressure measurements per result
         * A higher value increases precision
         */
        int16_t prs_osr = 1;
        Dps3xxPressureSensor->getIntStatusFifoFull();
        if (this->Dps3xxPressureSensor->startMeasureBothCont(temp_mr, temp_osr, prs_mr, prs_osr) != DPS__SUCCEEDED) {
          this->failure_reason_ += "Failed to start continuous measurement;";
          this->mark_failed();
          return;
        }
        this->failure_reason_ += "Failed to set interrupt sources;";
        this->mark_failed();
        return;
      }
    } else if (this->operation_mode_ == 0) {
      // Single-shot mode: use measurement ready interrupts
      if (this->Dps3xxPressureSensor->setInterruptSources(DPS3xx_BOTH_INTR, 0) != DPS__SUCCEEDED) {
        this->failure_reason_ += "Failed to set interrupt sources;";
        this->mark_failed();
        return;
      }
    } else {
      this->failure_reason_ += "Invalid operation mode configured;";
      this->mark_failed();
      return;
    }

    // Configure interrupts if pin is provided
    if (this->interrupt_pin_ != nullptr) {
      // Setup GPIO interrupt pin first
      this->interrupt_pin_->setup();
      this->interrupt_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
      this->interrupt_pin_->attach_interrupt(XensivDPS3xx::gpio_intr, this, gpio::INTERRUPT_FALLING_EDGE);
    } else {
      this->failure_reason_ += "interrupt pin not configured;";
    }
  });
}

void XensivDPS3xx::loop() {
  // Check if data is ready via interrupt
  // if (this->Dps3xxPressureSensor->getIntStatusFifoFull() == 1) {
  //   ESP_LOGD(TAG, "FIFO Full Interrupt occurred, data ready.");
  //   if (operation_mode_ == 1) {
  //     // In continuous mode, read all available samples from FIFO
  //     float temp_buffer[32];
  //     float prs_buffer[32];
  //     uint8_t temp_count = 32;
  //     uint8_t prs_count = 32;
  //     int16_t res = Dps3xxPressureSensor->getContResults(temp_buffer, temp_count, prs_buffer, prs_count);
  //     if (res != DPS__SUCCEEDED) {
  //       ESP_LOGW(TAG, "getContResults() returned: %d", res);
  //       return;
  //     }
  //     // Publish the last temperature sample
  //     if (this->temperature_sensor_ != nullptr && temp_count > 0) {
  //       this->temperature_sensor_->publish_state(temp_buffer[temp_count - 1]);
  //     }
  //     // Publish the last pressure sample
  //     if (this->pressure_sensor_ != nullptr && prs_count > 0) {
  //       this->pressure_sensor_->publish_state(prs_buffer[prs_count - 1] / 1000);  // Convert to hPa
  //     }
  //     return;
  //   }
  // }else if(this->Dps3xxPressureSensor->getIntStatusFifoFull() == -1){
  //   ESP_LOGW(TAG, "The FIFO IS Disabled: -1");
  //   return;
  // }

  if (data_ready_) {
    data_ready_ = false;
    ESP_LOGW(TAG, "Data ready interrupt occurred.");
    if (operation_mode_ == 1) {
      ESP_LOGW(TAG, "Reading data in continuous mode.");
      // In continuous mode, read all available samples from FIFO
      uint8_t pressureCount = 20;
      float pressure[pressureCount];
      uint8_t temperatureCount = 20;
      float temperature[temperatureCount];
      int16_t ret = Dps3xxPressureSensor->getContResults(temperature, temperatureCount, pressure, pressureCount);
      ESP_LOGW(TAG, "getContResults() returned: %d", ret);
    } else if (operation_mode_ == 0) {
      ESP_LOGW(TAG, "Reading data in single-shot mode.");
      // In single-shot mode, read one temperature and one pressure value
      float result;
      int16_t ret = Dps3xxPressureSensor->getSingleResult(result);
      if (ret != DPS__SUCCEEDED) {
        ESP_LOGW(TAG, "getSingleResult() returned: %d", ret);
        return;
      } else {
        this->pressure_sensor_->publish_state(result / 1000.0f);  // Convert to hPa
      }
    }
  }
}

void XensivDPS3xx::gpio_intr(XensivDPS3xx *arg) { arg->data_ready_ = true; }

/**
 * @brief Perform a single temperature measurement
 *
 * @return true
 * @return false
 */
bool XensivDPS3xx::measure_temperature_now() {
  ESP_LOGD(TAG, "Starting temperature measure now");
  float temperature;
  float pressure;
  uint8_t oversampling = 7;
  int16_t ret;

  ret = Dps3xxPressureSensor->startMeasureTempOnce(oversampling);
  if (ret != 0) {
    // Something went wrong.
    ESP_LOGW(TAG, "startMeasureTempOnce() returned: %d", ret);
    return false;
  }
  return true;
}

/**
 * @brief Perform a single pressure measurement
 *
 * @return true
 * @return false
 */
bool XensivDPS3xx::measure_pressure_now() {
  ESP_LOGD(TAG, "Starting pressure measure now");
  float temperature;
  float pressure;
  uint8_t oversampling = 7;
  int16_t ret;

  ret = Dps3xxPressureSensor->startMeasurePressureOnce(oversampling);
  if (ret != 0) {
    // Something went wrong.
    ESP_LOGW(TAG, "startMeasurePressureOnce() returned: %d", ret);
    return false;
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
