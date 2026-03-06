#include "spa06_base.h"

namespace esphome::spa06_base {

static const char *const TAG = "spa06";

static const LogString *oversampling_to_str(const Oversampling oversampling) {
  switch (oversampling) {
    case Oversampling::OVERSAMPLING_NONE:
      return LOG_STR("X1");
    case Oversampling::OVERSAMPLING_X2:
      return LOG_STR("X2");
    case Oversampling::OVERSAMPLING_X4:
      return LOG_STR("X4");
    case Oversampling::OVERSAMPLING_X8:
      return LOG_STR("X8");
    case Oversampling::OVERSAMPLING_X16:
      return LOG_STR("X16");
    case Oversampling::OVERSAMPLING_X32:
      return LOG_STR("X32");
    case Oversampling::OVERSAMPLING_X64:
      return LOG_STR("X64");
    case Oversampling::OVERSAMPLING_X128:
      return LOG_STR("X128");
    default:
      return LOG_STR("");
  }
};

static const LogString *meas_rate_to_str(SampleRate rate) {
  switch (rate) {
    case SampleRate::SAMPLE_RATE_1:
      return LOG_STR("1Hz");
    case SampleRate::SAMPLE_RATE_2:
      return LOG_STR("2Hz");
    case SampleRate::SAMPLE_RATE_4:
      return LOG_STR("4Hz");
    case SampleRate::SAMPLE_RATE_8:
      return LOG_STR("8Hz");
    case SampleRate::SAMPLE_RATE_16:
      return LOG_STR("16Hz");
    case SampleRate::SAMPLE_RATE_32:
      return LOG_STR("32Hz");
    case SampleRate::SAMPLE_RATE_64:
      return LOG_STR("64Hz");
    case SampleRate::SAMPLE_RATE_128:
      return LOG_STR("128Hz");
    case SampleRate::SAMPLE_RATE_25P16:
      return LOG_STR("1.5625Hz");
    case SampleRate::SAMPLE_RATE_25P8:
      return LOG_STR("3.125Hz");
    case SampleRate::SAMPLE_RATE_25P4:
      return LOG_STR("6.25Hz");
    case SampleRate::SAMPLE_RATE_25P2:
      return LOG_STR("12.5Hz");
    case SampleRate::SAMPLE_RATE_25:
      return LOG_STR("25Hz");
    case SampleRate::SAMPLE_RATE_50:
      return LOG_STR("50Hz");
    case SampleRate::SAMPLE_RATE_100:
      return LOG_STR("100Hz");
    case SampleRate::SAMPLE_RATE_200:
      return LOG_STR("200Hz");
    default:
      return LOG_STR("");
  }
}

// Sign extension function for <=16 bit types
inline int16_t decode16(uint8_t msb, uint8_t lsb, size_t bits, size_t head = 0) {
  return static_cast<int16_t>(encode_uint16(msb, lsb) << head) >> (16 - bits);
}

// Sign extension function for <=32 bit types
inline int32_t decode32(uint8_t xmsb, uint8_t msb, uint8_t lsb, uint8_t xlsb, size_t bits, size_t head = 0) {
  return static_cast<int32_t>(encode_uint32(xmsb, msb, lsb, xlsb) << head) >> (32 - bits);
}

void SPA06Component::dump_config() {
  ESP_LOGCONFIG(TAG, "SPA06:");
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  Measurement conversion time: %ums", this->conversion_time_);
  if (this->temperature_sensor_) {
    LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
    ESP_LOGCONFIG(TAG,
                  "  Oversampling: %s\n"
                  "  Rate: %s",
                  LOG_STR_ARG(oversampling_to_str(this->temperature_oversampling_)),
                  LOG_STR_ARG(meas_rate_to_str(this->temperature_rate_)));
  }
  if (this->pressure_sensor_) {
    LOG_SENSOR(" ", "Pressure", this->pressure_sensor_);

    ESP_LOGCONFIG(TAG,
                  "  Oversampling: %s\n"
                  "  Rate: %s",
                  LOG_STR_ARG(oversampling_to_str(this->pressure_oversampling_)),
                  LOG_STR_ARG(meas_rate_to_str(this->pressure_rate_)));
  }
}

void SPA06Component::setup() {
  // Startup sequence for SPA06 (Pg. 16, Figure 4.6.4):
  //   1. Perform a soft reset
  //   2. Verify sensor chip ID matches
  //   3. Verify coefficients are ready
  //   4. Read coefficients
  //   5. Configure temperature sensor
  //   6. Configure pressure sensor
  //   7. Configure FIFO
  //   8. Start sensor in background mode

  // 1. Soft reset
  if (!this->soft_reset_()) {
    ESP_LOGE(TAG, "Reset failed");
    // this->error_code_ = ERROR_SENSOR_RESET;
    this->mark_failed();
    return;
  }

  // reset() internally delays by 2ms to make sure that
  // the sensor is in a ready state. We need to delay at
  // least 1 more millisecond before coefficients are
  // ready (datasheet pg. 7). This code spreads those two
  // milliseconds out before and after reading the chip ID.
  delay(1);

  // 2. Read chip ID
  // TODO: check ID for consistency?
  if (!spa_read_byte(SPA06_ID, &this->prod_id_.reg)) {
    ESP_LOGE(TAG, "Chip ID read failure");
    // this->error_code = ERROR_CHIP_ID_READ;
    this->mark_failed();
    return;
  }
  ESP_LOGV(TAG,
           "Product Info:\n"
           "  Prod ID: %u\n"
           "  Rev ID: %u",
           this->prod_id_.bit.prod_id, this->prod_id_.bit.rev_id);
  // One more delay before coefficients should be ready
  delay(2);

  // 3. Read chip readiness from CFG_REG
  //    Only fail here if the sensor coefficients are not ready
  if (!spa_read_byte(SPA06_MEAS_CFG, &this->meas_.reg)) {
    ESP_LOGD(TAG, "Sensor not ready, attempting to continue");
  }
  if (!meas_.bit.coef_ready) {
    ESP_LOGE(TAG, "Coefficients not ready");
    // this->error_code = ERROR_CHIP_COEF_NOT_READY;
    this->mark_failed();
    return;
  }

  // 4. Read coefficients
  if (!this->read_coefficients_()) {
    ESP_LOGE(TAG, "Coefficient read error");
    this->mark_failed();
    return;
  }

  // 5. Configure temperature and pressure sensors
  //   Default to measuring both temperature and pressure

  // Temperature must be read regardless of configuration to compute pressure
  // If temperature is not configured in config:
  // - No oversampling is used
  // - Lowest possible rate is configured
  if (!this->temperature_sensor_) {
    this->temperature_rate_ = SAMPLE_RATE_1;
    this->temperature_oversampling_ = OVERSAMPLING_NONE;
  }

  // If pressure is not configured in config
  // - No oversampling is used
  // - Lowest possible rate is configured
  if (!this->pressure_sensor_) {
    this->pressure_rate_ = SAMPLE_RATE_1;
    this->pressure_oversampling_ = OVERSAMPLING_NONE;
  }

  // Write temperature settings
  if (!write_temperature_settings_(this->temperature_oversampling_, this->temperature_rate_)) {
    ESP_LOGE(TAG, "Temperature settings write fail");
    this->mark_failed();
    return;
  }

  // Write pressure settings
  if (!write_pressure_settings_(this->pressure_oversampling_, this->pressure_rate_)) {
    ESP_LOGE(TAG, "Pressure settings write fail");
    this->mark_failed();
    return;
  }
  // Write communication settings
  // This call sets the bit shifts for pressure and temperature if
  //   their respective oversampling config is > X8
  // This call also disables interrupts FIFO and specifies SPI 4-wire
  if (!write_communication_settings_(this->pressure_oversampling_ > OVERSAMPLING_X8,
                                     this->temperature_oversampling_ > OVERSAMPLING_X8)) {
    ESP_LOGE(TAG, "Comm settings write fail");
    this->mark_failed();
    return;
  }

  // Write measurement settings
  // This function sets background measurement mode without FIFO
  if (!write_measurement_settings_(this->pressure_sensor_ ? MeasCrtl::MEASCRTL_BG_BOTH : MeasCrtl::MEASCRTL_BG_TEMP)) {
    ESP_LOGE(TAG, "Measurement settings write fail");
    this->mark_failed();
    return;
  }
}

bool SPA06Component::write_temperature_settings_(Oversampling oversampling, SampleRate rate) {
  return this->write_sensor_settings_(oversampling, rate, SPA06_TMP_CFG);
}

bool SPA06Component::write_pressure_settings_(Oversampling oversampling, SampleRate rate) {
  return this->write_sensor_settings_(oversampling, rate, SPA06_PSR_CFG);
}

bool SPA06Component::write_sensor_settings_(Oversampling oversampling, SampleRate rate, uint8_t reg) {
  if (reg != SPA06_PSR_CFG and reg != SPA06_TMP_CFG) {
    return false;
  }
  this->pt_meas_cfg_.bit.rate = rate;
  this->pt_meas_cfg_.bit.prc = oversampling;
  ESP_LOGD(TAG, "Config write: %02x", this->pt_meas_cfg_.reg);
  return spa_write_byte(reg, this->pt_meas_cfg_.reg);
}

bool SPA06Component::write_measurement_settings_(MeasCrtl crtl) {
  this->meas_.bit.meas_crtl = crtl;
  return spa_write_byte(SPA06_MEAS_CFG, this->meas_.reg);
}

bool SPA06Component::write_communication_settings_(bool pressure_shift, bool temperature_shift, bool interrupt_hl,
                                                   bool interrupt_fifo, bool interrupt_tmp, bool interrupt_prs,
                                                   bool enable_fifo, bool spi_3wire) {
  this->cfg_.bit.p_shift = pressure_shift;
  this->cfg_.bit.t_shift = temperature_shift;
  this->cfg_.bit.int_hl = interrupt_hl;
  this->cfg_.bit.int_fifo = interrupt_fifo;
  this->cfg_.bit.int_tmp = interrupt_tmp;
  this->cfg_.bit.int_prs = interrupt_prs;
  this->cfg_.bit.fifo_en = enable_fifo;
  this->cfg_.bit.spi_3wire = spi_3wire;
  return spa_write_byte(SPA06_CFG_REG, this->cfg_.reg);
}

bool SPA06Component::read_coefficients_() {
  uint8_t coef[SPA06_COEF_LEN];
  if (!spa_read_bytes(SPA06_COEF, coef, SPA06_COEF_LEN)) {
    return false;
  }
  this->c0_ = decode16(coef[0], coef[1], 12);
  this->c1_ = decode16(coef[1], coef[2], 12, 4);
  this->c00_ = decode32(coef[3], coef[4], coef[5], 0, 20);
  this->c10_ = decode32(coef[5], coef[6], coef[7], 0, 20, 4);
  this->c01_ = decode16(coef[8], coef[9], 16);
  this->c11_ = decode16(coef[10], coef[11], 16);
  this->c20_ = decode16(coef[12], coef[13], 16);
  this->c21_ = decode16(coef[14], coef[15], 16);
  this->c30_ = decode16(coef[16], coef[17], 16);
  this->c31_ = decode16(coef[18], coef[19], 12);
  this->c40_ = decode16(coef[19], coef[20], 12, 4);

  ESP_LOGV(TAG,
           "Coefficients:\n"
           "  c0: %i, c1: %i,\n"
           "  c00: %i, c10: %i, c20: %i, c30: %i, c40: %i,\n"
           "  c01: %i, c11: %i, c21: %i, c31: %i",
           this->c0_, this->c1_, this->c00_, this->c10_, this->c20_, this->c30_, this->c01_, this->c11_, this->c21_,
           this->c31_);
  return true;
}

bool SPA06Component::soft_reset_() {
  // Setup steps for SPA06:
  // 1. Perform a protocol reset (required to write command for SPI code, noop for I2C)
  this->protocol_reset_();

  // 2. Perform the actual reset
  this->reset_.bit.fifo_flush = true;
  this->reset_.bit.soft_rst = SPA06_SOFT_RESET;
  if (!this->spa_write_byte(SPA06_RESET, this->reset_.reg)) {
    return false;
  }

  // 3. Wait for chip to become ready. Datasheet specifies 2 seconds; wait 3
  delay(3);
  // 4. Perform another protocol reset (required for SPI code, noop for I2C)
  this->protocol_reset_();
  return true;
}

// Temperature conversion formula. See datasheet pg. 14
float SPA06Component::convert_temperature_(const float &t_raw_sc) { return this->c0_ * 0.5 + this->c1_ * t_raw_sc; }
// Pressure conversion formula. See datasheet pg. 14
float SPA06Component::convert_pressure_(const float &p_raw_sc, const float &t_raw_sc) {
  float p2_raw_sc = std::pow(p_raw_sc, 2);
  float p3_raw_sc = std::pow(p_raw_sc, 3);
  float p4_raw_sc = std::pow(p_raw_sc, 4);
  return this->c00_ + (float) this->c10_ * p_raw_sc + (float) this->c20_ * p2_raw_sc + (float) this->c30_ * p3_raw_sc +
         (float) this->c40_ * p4_raw_sc +
         t_raw_sc * ((float) this->c01_ + (float) this->c11_ * p_raw_sc + (float) this->c21_ * p2_raw_sc +
                     (float) this->c31_ * p3_raw_sc);
}

void SPA06Component::update() {
  // Verify either a temperature or pressure sensor is defined before proceeding
  if ((!this->temperature_sensor_) && (!this->pressure_sensor_)) {
    return;
  }

  // Queue a background task for retrieving the measurement
  this->set_timeout("measurement", this->conversion_time_, [this]() {
    float raw_temperature;
    float temperature = 0.0;
    float pressure = 0.0;
    if (this->pressure_sensor_) {
      if (!this->read_temperature_and_pressure_(temperature, pressure, raw_temperature)) {
        ESP_LOGW(TAG, "Temperature and pressure read failure");
        this->status_set_warning();
        return;
      }
    } else {
      if (!this->read_temperature_(temperature, raw_temperature)) {
        ESP_LOGW(TAG, "Temperature read fail");
        this->status_set_warning();
        return;
      }
    }
    if (this->temperature_sensor_) {
      this->temperature_sensor_->publish_state(temperature);
    } else {
      ESP_LOGD(TAG, "No temperature sensor configured?");
    }
    if (this->pressure_sensor_) {
      this->pressure_sensor_->publish_state(pressure);
    } else {
      ESP_LOGD(TAG, "No pressure sensor configured?");
    }
    this->status_clear_warning();
  });
}

bool SPA06Component::read_temperature_and_pressure_(float &temperature, float &pressure, float &t_raw_sc) {
  // 1. Check measurement register for readiness
  if (!this->spa_read_byte(SPA06_MEAS_CFG, &this->meas_.reg)) {
    ESP_LOGD(TAG, "Cannot read meas config");
    return false;
  }
  // Exit if temp and pressure readiness aren't ready
  if (!this->meas_.bit.prs_ready or !this->meas_.bit.tmp_ready) {
    return false;
  }
  // Temperature read and decode
  if (!this->read_temperature_(temperature, t_raw_sc)) {
    return false;
  }
  // Read raw pressure from device
  if (!this->spa_read_bytes(SPA06_PSR, this->psr_tmp_read_, 3)) {
    return false;
  }
  // Calculate raw scaled pressure value
  float p_raw_sc = (float) decode32(psr_tmp_read_[0], psr_tmp_read_[1], psr_tmp_read_[2], 0, 24) / (float) this->kp_;

  // Calculate full pressure values
  pressure = this->convert_pressure_(p_raw_sc, t_raw_sc);
  return true;
}

bool SPA06Component::read_temperature_(float &temperature, float &t_raw_sc) {
  if (!this->spa_read_bytes(SPA06_TMP, this->psr_tmp_read_, 3)) {
    return false;
  }

  t_raw_sc = (float) decode32(psr_tmp_read_[0], psr_tmp_read_[1], psr_tmp_read_[2], 0, 24) / (float) this->kt_;
  temperature = this->convert_temperature_(t_raw_sc);
  return true;
}
}  // namespace esphome::spa06_base
