#include "pas_co2.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pas_co2 {

static const char *const TAG = "pas_co2";

static const uint8_t PASCO2_REG_PRODUCT_ID = 0x0;
static const uint8_t PASCO2_REG_SENSOR_STATUS = 0x1;
static const uint8_t PASCO2_REG_MEASUREMENT_RATE = 0x2;
static const uint8_t PASCO2_REG_MODE = 0x4;
static const uint8_t PASCO2_REG_CO2 = 0x5;
static const uint8_t PASCO2_REG_MEASUREMENT_STATUS = 0x7;
static const uint8_t PASCO2_REG_PRESSURE = 0xB;
static const uint8_t PASCO2_REG_CALIB_REF = 0xD;
static const uint8_t PASCO2_REG_SCRATCH_PAD = 0xF;
static const uint8_t PASCO2_REG_SOFTRESET = 0x10;
static const uint8_t PASCO2_CMD_RESET_SENSOR = 0xA3;
static const uint8_t PASCO2_CMD_SAVE_FCS_CALIB_OFFSET = 0xCF;

std::string sensor_status_to_string(uint8_t status) {
  std::string result;
  if (status & 1 << 3)
    result = "Communication error\n";
  if (status & 1 << 4)
    result += "Out-of-range VDD12V error\n";
  if (status & 1 << 5)
    result += "Out-of-range temperature error\n";
  if (status & 1 << 6)
    result += "PWM_DIS pin status\n";
  if (status & 1 << 7) {
    result += "The sensor has been initialized correctly";
  } else {
    result += "The sensor has not been initialized correctly";
  }
  return result;
}

void PasCo2Component::setup() {
  ESP_LOGD(TAG, "Setting up PAS Co2 Sensor");
  this->write_register_(PASCO2_REG_SOFTRESET, PASCO2_CMD_RESET_SENSOR);
  this->set_timeout(2000, [this]() {
    // Get sensor status
    uint8_t status;
    if (i2c::ERROR_OK != this->read_register_(PASCO2_REG_SENSOR_STATUS, status)) {
      ESP_LOGE(TAG, "Failed to read sensor status");
      this->mark_failed();
      return;
    }

    // Get product id
    if (i2c::ERROR_OK != this->read_register_(PASCO2_REG_SENSOR_STATUS, this->product_id_)) {
      ESP_LOGE(TAG, "Failed to read product id");
      this->mark_failed();
      return;
    }

    // Set calibration offset
    if (this->calib_ref_ > 0) {
      if (i2c::ERROR_OK != this->write_register_(PASCO2_REG_CALIB_REF, this->calib_ref_)) {
        ESP_LOGE(TAG, "Failed to set calibration offset");
        this->mark_failed();
        return;
      }
    }

    // get calibration offset
    if (i2c::ERROR_OK != this->read_register_(PASCO2_REG_CALIB_REF, this->calib_ref_)) {
      ESP_LOGE(TAG, "Failed to read calibration offset");
      this->mark_failed();
      return;
    }

    measurement_config_t cfg;
    cfg.b.op_mode = OP_MODE_IDLE;
    cfg.b.boc_cfg = this->enable_abc_ ? BOC_CFG_AUTOMATIC : BOC_CFG_DISABLE;
    // Set idle mode
    if (i2c::ERROR_OK != this->write_register_(PASCO2_REG_MODE, cfg.u)) {
      ESP_LOGE(TAG, "Failed to set operation mode");
      this->mark_failed();
      return;
    }

    this->set_timeout(500, [this]() {
      if (this->ambient_pressure_ > 0) {
        // Set ambient pressure
        if (!this->update_ambient_pressure_compensation_(this->ambient_pressure_)) {
          ESP_LOGE(TAG, "Failed to set pressure compensation");
          this->mark_failed();
          return;
        }
      }

      // Set measurement rate to align with sensor update_interval
      if (i2c::ERROR_OK != this->set_measurement_rate_(this->update_interval_ / 1000)) {
        ESP_LOGE(TAG, "Failed to set measurement interval");
        this->mark_failed();
        return;
      }

      // Set continous mode
      measurement_config_t cfg;
      cfg.b.op_mode = OP_MODE_CONTINUOUS;
      cfg.b.boc_cfg = this->enable_abc_ ? BOC_CFG_AUTOMATIC : BOC_CFG_DISABLE;
      if (i2c::ERROR_OK != this->write_register_(PASCO2_REG_MODE, cfg.u)) {
        ESP_LOGE(TAG, "Failed to set operation mode");
        this->mark_failed();
        return;
      }
      initialized_ = true;
    });
  });
}

void PasCo2Component::update() {
  // pressure compensation
  if (this->ambient_pressure_source_ != nullptr) {
    float pressure = this->ambient_pressure_source_->state;
    if (!std::isnan(pressure)) {
      set_ambient_pressure_compensation(pressure);
    }
  }

  // Get sensor status
  uint8_t status;
  static uint8_t error_count = 0;
  // Fails intermittently - send a "soft reset" command to the sensor to recover
  if (i2c::ERROR_OK != this->read_register_(PASCO2_REG_MEASUREMENT_STATUS, status)) {
    error_count++;
    if (error_count == 3) {
      ESP_LOGW(TAG, "%d read errors in a row. Resetting the sensor.", error_count);
      this->write_register_(PASCO2_REG_SOFTRESET, PASCO2_CMD_RESET_SENSOR);
      this->set_timeout(2000, [this]() {
        // Set continous mode
        measurement_config_t cfg;
        cfg.b.op_mode = OP_MODE_CONTINUOUS;
        cfg.b.boc_cfg = this->enable_abc_ ? BOC_CFG_AUTOMATIC : BOC_CFG_DISABLE;
        if (i2c::ERROR_OK != this->write_register_(PASCO2_REG_MODE, cfg.u)) {
          ESP_LOGE(TAG, "Failed to set operation mode");
          this->mark_failed();
        } else
          error_count = 0;
      });
    }
    uint8_t sensor_status;
    auto i2c_err = this->read_register_(PASCO2_REG_SENSOR_STATUS, sensor_status);
    if (i2c::ERROR_OK != i2c_err) {
      ESP_LOGW(TAG, "Failed to get sensor status err=%d", i2c_err);
    } else {
      ESP_LOGI(TAG, "Sensor status %d (0x%x)\n%s", sensor_status, sensor_status,
               sensor_status_to_string(sensor_status).c_str());
    }
    ESP_LOGW(TAG, "Failed to read measurement status");
    this->status_set_warning();
    return;
  }
  error_count = 0;

  static bool initalizing = true;
  if ((status & 0x10) != 0x10) {
    // sensors needs some time for startup
    // surpress warning until the first data is received
    if (initalizing) {
      ESP_LOGI(TAG, "CO2 Sensor is still initalizing.");
    } else {
      ESP_LOGW(TAG, "Data not ready (%d)", status);
    }
    return;
  }
  initalizing = false;
  uint16_t co2;
  if (i2c::ERROR_OK != this->read_register_(PASCO2_REG_CO2, co2)) {
    ESP_LOGE(TAG, "Failed to read co2");
    this->status_set_warning();
    return;
  }
  this->status_clear_warning();
  ESP_LOGD(TAG, "PasCo2 CO₂=%uppm", co2);
  if (this->co2_sensor_ != nullptr)
    this->co2_sensor_->publish_state(co2);
}

void PasCo2Component::dump_config() {
  ESP_LOGCONFIG(TAG, "PasCo2:");
  ESP_LOGCONFIG(TAG, "  Product id: %d.%d", this->product_id_ >> 4, this->product_id_ & 0xF);
  ESP_LOGCONFIG(TAG, "  Automatic background calibration: %s", ONOFF(this->enable_abc_));
  if (this->ambient_pressure_compensation_) {
    ESP_LOGCONFIG(TAG, "  Ambient pressure compensation: %d hPa", this->ambient_pressure_);
  }
  ESP_LOGCONFIG(TAG, "  Calibration offset: %d ppm", this->calib_ref_);

  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
}

// Runs the sensor for 3 measurements on forced calibration mode
// and then restores the prevoius settings
void PasCo2Component::start_foced_calibration() {
  measurement_config_t cfg;

  if (i2c::ERROR_OK != this->read_register_(PASCO2_REG_MEASUREMENT_STATUS, &cfg.u, 1)) {
    ESP_LOGW(TAG, "Failed to get measurement status");
    this->status_set_warning();
    return;
  }
  cfg.b.op_mode = OP_MODE_IDLE;
  if (i2c::ERROR_OK != this->write_register_(PASCO2_REG_MODE, cfg.u)) {
    ESP_LOGE(TAG, "Failed to set measurement configuration");
    this->status_set_warning();
    return;
  }

  // Set measurement rate
  if (i2c::ERROR_OK != this->set_measurement_rate_(10)) {
    ESP_LOGE(TAG, "Failed to set measurement interval");
    this->status_set_warning();
    return;
  }

  ESP_LOGI(TAG, "Starting forced CO2 calibration");

  // Set calibration offset
  if (this->calib_ref_ > 0) {
    if (i2c::ERROR_OK != this->write_register_(PASCO2_REG_CALIB_REF, this->calib_ref_)) {
      ESP_LOGE(TAG, "Failed to set calibration offset");
      this->status_set_warning();
      return;
    }
  }

  cfg.b.boc_cfg = BOC_CFG_FORCED;
  cfg.b.op_mode = OP_MODE_CONTINUOUS;
  if (i2c::ERROR_OK != this->write_register_(PASCO2_REG_MODE, cfg.u)) {
    ESP_LOGE(TAG, "Failed to set measurement configuration");
    this->status_set_warning();
    return;
  }
  delay(100);  // NOLINT
  this->set_retry(5000, 250, [this]() {
    // Get measurement status
    measurement_config_t cfg;
    if (i2c::ERROR_OK == this->read_register_(PASCO2_REG_MODE, cfg.u)) {
      if (cfg.b.boc_cfg == BOC_CFG_FORCED) {
        ESP_LOGI(TAG, "Forced calibration ongoing (0x%X)", cfg.u);
        return RETRY;
      } else {
        ESP_LOGI(TAG, "Forced calibration complete - restoring measurement mode");
        cfg.b.op_mode = OP_MODE_IDLE;
        if (i2c::ERROR_OK != this->write_register_(PASCO2_REG_MODE, cfg.u)) {
          ESP_LOGE(TAG, "Failed to set measurement configuration");
          this->status_set_warning();
        }
        delay(400);  // NOLINT
        auto cmd = PASCO2_CMD_SAVE_FCS_CALIB_OFFSET;
        this->write_register_(PASCO2_REG_SOFTRESET, cmd);
        delay(5);
        // Set measurement rate to align with sensor update_interval
        if (i2c::ERROR_OK != this->set_measurement_rate_(this->update_interval_ / 1000)) {
          ESP_LOGE(TAG, "Failed to set measurement interval");
          this->status_set_warning();
        }
        delay(5);
        // Set continous mode
        cfg.b.op_mode = OP_MODE_CONTINUOUS;
        cfg.b.boc_cfg = this->enable_abc_ ? BOC_CFG_AUTOMATIC : BOC_CFG_DISABLE;
        if (i2c::ERROR_OK != this->write_register_(PASCO2_REG_MODE, cfg.u)) {
          ESP_LOGE(TAG, "Failed to set operation mode");
          this->status_set_warning();
        }
        return DONE;
      }
    }
    return RETRY;
  });
}

i2c::ErrorCode PasCo2Component::set_measurement_rate_(uint16_t rate_in_seconds) {
  uint16_t interval = i2c::htoi2cs(rate_in_seconds);
  return this->write_register_(PASCO2_REG_MEASUREMENT_RATE, interval);
}

void PasCo2Component::set_ambient_pressure_compensation(float pressure_in_hpa) {
  ambient_pressure_compensation_ = true;

  // remove millibar from comparison to avoid frequent updates +/- 10 millibar doesn't matter
  if (initialized_ && (static_cast<uint16_t>(pressure_in_hpa) / 10 != ambient_pressure_ / 10)) {
    if (update_ambient_pressure_compensation_(pressure_in_hpa)) {
      ambient_pressure_ = pressure_in_hpa;
      ESP_LOGD(TAG, "ambient pressure compensation set to %d hPa", ambient_pressure_);
    } else {
      ESP_LOGE(TAG, "Failed to set pressure compensation %0.0f hPa", pressure_in_hpa);
      this->status_set_warning();
    }
  } else {
    ESP_LOGV(TAG, "ambient pressure compensation skipped - no change required");
  }
}

bool PasCo2Component::update_ambient_pressure_compensation_(uint16_t pressure_in_hpa) {
  // Set ambient pressure
  return i2c::ERROR_OK == this->write_register_(PASCO2_REG_PRESSURE, pressure_in_hpa);
}

bool PasCo2Component::update_abc_enable(bool enable) {
  // Set continous mode
  measurement_config_t cfg;

  if (i2c::ERROR_OK != this->read_register_(PASCO2_REG_MODE, cfg.u)) {
    ESP_LOGE(TAG, "Failed to get operation mode");
    return false;
  }

  // Switch to idle mode for 400 ms
  cfg.b.op_mode = OP_MODE_IDLE;
  if (i2c::ERROR_OK != this->write_register_(PASCO2_REG_MODE, cfg.u)) {
    ESP_LOGE(TAG, "Failed to set idle mode");
    return false;
  }
  delay(400);  // NOLINT (updating abc is a very rare operation)

  this->enable_abc_ = enable;
  cfg.b.boc_cfg = this->enable_abc_ ? BOC_CFG_AUTOMATIC : BOC_CFG_DISABLE;
  cfg.b.op_mode = OP_MODE_CONTINUOUS;
  // restore previous mode
  return i2c::ERROR_OK == this->write_register_(PASCO2_REG_MODE, cfg.u);
}

}  // namespace pas_co2
}  // namespace esphome
