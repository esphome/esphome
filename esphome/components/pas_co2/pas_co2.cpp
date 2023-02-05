#include "pas_co2.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "xensiv_pasco2_regs.h"

namespace esphome {
namespace pas_co2 {

static const char *const TAG = "pas_co2";

void PASCO2Component::setup() {
  ESP_LOGD(TAG, "Setting up...");

  this->initialized_ = false;

  // The sensor needs 1000 ms to enter the idle state.
  this->set_timeout(10000, [this]() { this->init_(); });
}

void PASCO2Component::dump_config() {
  ESP_LOGCONFIG(TAG, "pas_co2:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    switch (this->error_code_) {
      case NO_ERROR:
        ESP_LOGW(TAG, "No error");
        break;
      case COMMUNICATION_FAILED:
        ESP_LOGW(TAG, "Communication failed! Is the sensor connected?");
        break;
      case SOFT_RESET_FAILED:
        ESP_LOGW(TAG, "Soft reset failed!");
        break;
      case MEASUREMENT_INIT_FAILED:
        ESP_LOGW(TAG, "Measurement initialization failed!");
        break;
      case UNKNOWN:
        ESP_LOGW(TAG, "Unknown setup error!");
        break;
    }
  }
  if (this->ambient_pressure_source_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Dynamic ambient pressure compensation using sensor '%s'",
                  this->ambient_pressure_source_->get_name().c_str());
  } else {
    if (this->ambient_pressure_compensation_) {
      ESP_LOGCONFIG(TAG, "  Ambient pressure compensation: %dmBar", this->ambient_pressure_);
    } else {
      ESP_LOGCONFIG(TAG, "  Ambient pressure compensation disabled");
    }
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
}

void PASCO2Component::update() {
  if (!initialized_) {
    ESP_LOGW(TAG, "Update is called, when sensor has not been initialized yet");
    return;
  }

  if (this->ambient_pressure_source_ != nullptr) {
    const float pressure = this->ambient_pressure_source_->state / 1000.0f;
    if (!std::isnan(pressure)) {
      this->set_ambient_pressure_compensation(this->ambient_pressure_source_->state / 1000.0f);
    }
  }

  if (this->retry_count_ == 0) {
    this->try_read_measurement_();
  } else {
    ESP_LOGW(TAG, "Skipping update interval because already retrying");
  }
}

// Note pressure in bar here. Convert to hPa
void PASCO2Component::set_ambient_pressure_compensation(float pressure_in_bar) {
  this->ambient_pressure_compensation_ = true;
  const uint16_t new_ambient_pressure = (uint16_t)(pressure_in_bar * 1000);
  // Ignore changes less than 10 millibar, it doesn't matter.
  if (this->initialized_ && abs(new_ambient_pressure - this->ambient_pressure_) < 10) {
    this->update_ambient_pressure_compensation_(new_ambient_pressure);
    this->ambient_pressure_ = new_ambient_pressure;
  } else {
    ESP_LOGD(TAG, "Ambient pressure compensation skipped - no change required");
  }
}

void PASCO2Component::init_() {
  ESP_LOGD(TAG, "Initializing...");

  if (!this->test_scratch_register_()) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }

  uint8_t prod_id;
  if (this->read_byte(XENSIV_PASCO2_REG_PROD_ID, &prod_id)) {
    ESP_LOGD(TAG, "PROD_ID: %02x", prod_id);
  } else {
    ESP_LOGW(TAG, "Failed to read PROD_ID");
  }

  if (!this->write_byte(XENSIV_PASCO2_REG_SENS_RST, XENSIV_PASCO2_CMD_SOFT_RESET)) {
    ESP_LOGE(TAG, "Sending soft reset failed");
    this->error_code_ = SOFT_RESET_FAILED;
    this->mark_failed();
    return;
  }
  this->set_timeout(XENSIV_PASCO2_SOFT_RESET_DELAY_MS, [this]() {
    if (!this->check_sensor_status_()) {
      this->error_code_ = SOFT_RESET_FAILED;
      this->mark_failed();
      return;
    }

    // It seems that sensor waits setting IDLE and accepts SINGLE only after that.
    if (!this->set_mode_(SENSOR_MODE_IDLE) || !this->set_mode_(SENSOR_MODE_SINGLE)) {
      ESP_LOGE(TAG, "Failed to set first read sensor mode");
      this->error_code_ = MEASUREMENT_INIT_FAILED;
      this->mark_failed();
      return;
    }

    this->set_timeout(XENSIV_PASCO2_SINGLE_SHOT_DELAY_MS, [this]() {
      if (!read_measurement_()) {
        ESP_LOGW(TAG, "Failed to read first shot");
      }
      if (!this->set_rate_(this->get_update_interval() / 1000)) {
        ESP_LOGE(TAG, "Failed to enable continuous mode");
        this->error_code_ = MEASUREMENT_INIT_FAILED;
        this->mark_failed();
        return;
      }
      this->initialized_ = true;
      ESP_LOGD(TAG, "Sensor initialized");
    });
  });
}

bool PASCO2Component::test_scratch_register_() {
  // Check communication using scratch register that preserves value.
  if (!this->write_byte(XENSIV_PASCO2_REG_SCRATCH_PAD, XENSIV_PASCO2_COMM_TEST_VAL)) {
    ESP_LOGE(TAG, "Write to scratch register failed");
    return false;
  }

  uint8_t data = 0;
  if (!this->read_byte(XENSIV_PASCO2_REG_SCRATCH_PAD, &data)) {
    ESP_LOGE(TAG, "Read from scratch register failed");
    return false;
  }

  if (data != XENSIV_PASCO2_COMM_TEST_VAL) {
    ESP_LOGE(TAG, "Test value %02x in scratch register doesn't match, got %02x", XENSIV_PASCO2_COMM_TEST_VAL, data);
    return false;
  }
  return true;
}

bool PASCO2Component::check_sensor_status_() {
  uint8_t status = 0xff;
  if (!this->read_byte(XENSIV_PASCO2_REG_SENS_STS, &status)) {
    ESP_LOGE(TAG, "Reading SENS_STS failed");
    return false;
  }

  if ((status & XENSIV_PASCO2_REG_SENS_STS_ICCER_MSK) != 0) {
    ESP_LOGE(TAG, "Unexpected SENS_STS after soft reset: ICCERR");
    return false;
  } else if ((status & XENSIV_PASCO2_REG_SENS_STS_ORVS_MSK) != 0) {
    ESP_LOGE(TAG, "Unexpected SENS_STS after soft reset: ORVS");
    return false;
  } else if ((status & XENSIV_PASCO2_REG_SENS_STS_ORTMP_MSK) != 0) {
    ESP_LOGE(TAG, "Unexpected SENS_STS after soft reset: ORTMP");
    return false;
  } else if ((status & XENSIV_PASCO2_REG_SENS_STS_SEN_RDY_MSK) == 0) {
    ESP_LOGE(TAG, "Unexpected SENS_STS after soft reset: NOT RDY");
    return false;
  }

  return true;
}

bool PASCO2Component::set_mode_(SensorMode mode) {
  xensiv_pasco2_measurement_config_t cfg;
  if (!this->read_byte(XENSIV_PASCO2_REG_MEAS_CFG, &cfg.u)) {
    ESP_LOGE(TAG, "Failed to read: REG_MEAS_CFG");
    return false;
  }
  cfg.b.op_mode = mode;
  if (!this->write_byte(XENSIV_PASCO2_REG_MEAS_CFG, cfg.u)) {
    ESP_LOGE(TAG, "Failed to write: REG_MEAS_CFG");
    return false;
  }
  return true;
}

bool PASCO2Component::set_rate_(uint16_t rate_sec) {
  rate_sec = std::max(rate_sec, (uint16_t)XENSIV_PASCO2_MEAS_RATE_MIN);
  rate_sec = std::min(rate_sec, (uint16_t)XENSIV_PASCO2_MEAS_RATE_MAX);
  if (!this->write_byte_16(XENSIV_PASCO2_REG_MEAS_RATE_H, rate_sec)) {
    ESP_LOGE(TAG, "Failed to write: XENSIV_PASCO2_REG_MEAS_RATE_H");
    return false;
  }
  return this->set_mode_(SENSOR_MODE_CONTINUOUS);
}

bool PASCO2Component::update_ambient_pressure_compensation_(uint16_t pressure_in_hpa) {
  if (this->write_byte_16(XENSIV_PASCO2_REG_PRESS_REF_H, pressure_in_hpa)) {
    ESP_LOGD(TAG, "Setting ambient pressure compensation to %d hPa", pressure_in_hpa);
    return true;
  } else {
    ESP_LOGE(TAG, "Error setting ambient pressure compensation.");
    return false;
  }
  return true;
}

bool PASCO2Component::read_measurement_() {
  xensiv_pasco2_meas_status_t status;
  if (!this->read_byte(XENSIV_PASCO2_REG_MEAS_STS, &status.u)) {
    ESP_LOGE(TAG, "Reading MEAS_STS failed");
    return false;
  }

  if (!status.b.drdy) {
    return false;
  }

  uint16_t val;
  if (!this->read_byte_16(XENSIV_PASCO2_REG_CO2PPM_H, &val)) {
    ESP_LOGE(TAG, "Error reading measurement!");
    this->clear_status_();
    return false;
  }
  this->clear_status_();

  if (this->co2_sensor_ != nullptr)
    this->co2_sensor_->publish_state(val);

  return true;
}

void PASCO2Component::try_read_measurement_() {
  if (this->read_measurement_()) {
    this->retry_count_ = 0;
    this->status_clear_warning();
  } else if (++this->retry_count_ <= 3) {
    ESP_LOGD(TAG, "Measurement is not read, retrying...");
    this->set_timeout(1000, [this]() { this->try_read_measurement_(); });
  } else {
    ESP_LOGW(TAG, "Too many retries reading sensor, skipping update interval!");
    this->status_set_warning();
    this->retry_count_ = 0;
  }
}

bool PASCO2Component::clear_status_() {
  if (!this->write_byte(XENSIV_PASCO2_REG_MEAS_STS,
                        XENSIV_PASCO2_REG_MEAS_STS_INT_STS_CLR_MSK | XENSIV_PASCO2_REG_MEAS_STS_ALARM_CLR_MSK)) {
    ESP_LOGE(TAG, "Failed to clear measurement status in REG_MEAS_STS");
    return false;
  }
  return true;
}

}  // namespace pas_co2
}  // namespace esphome
