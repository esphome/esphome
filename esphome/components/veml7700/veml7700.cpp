#include "veml7700.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome {
namespace veml7700 {

static const char *const TAG = "veml7700";
static const size_t VEML_REG_SIZE = 2;

static float reduce_to_zero(float a, float b) { return (a > b) ? (a - b) : 0; }

template<typename T, size_t size> T get_next(const T (&array)[size], const T val) {
  size_t i = 0;
  size_t idx = -1;
  while (idx == -1 && i < size) {
    if (array[i] == val) {
      idx = i;
      break;
    }
    i++;
  }
  if (idx == -1 || i + 1 >= size)
    return val;
  return array[i + 1];
}

template<typename T, size_t size> T get_prev(const T (&array)[size], const T val) {
  size_t i = size - 1;
  size_t idx = -1;
  while (idx == -1 && i > 0) {
    if (array[i] == val) {
      idx = i;
      break;
    }
    i--;
  }
  if (idx == -1 || i == 0)
    return val;
  return array[i - 1];
}

static uint16_t get_itime_ms(IntegrationTime time) {
  uint16_t ms = 0;
  switch (time) {
    case INTEGRATION_TIME_100MS:
      ms = 100;
      break;
    case INTEGRATION_TIME_200MS:
      ms = 200;
      break;
    case INTEGRATION_TIME_400MS:
      ms = 400;
      break;
    case INTEGRATION_TIME_800MS:
      ms = 800;
      break;
    case INTEGRATION_TIME_50MS:
      ms = 50;
      break;
    case INTEGRATION_TIME_25MS:
      ms = 25;
      break;
    default:
      ms = 100;
  }
  return ms;
}

static float get_gain_coeff(Gain gain) {
  static const float GAIN_FLOAT[GAINS_COUNT] = {1.0f, 2.0f, 0.125f, 0.25f};
  return GAIN_FLOAT[gain & 0b11];
}

static const char *get_gain_str(Gain gain) {
  static const char *gain_str[GAINS_COUNT] = {"1x", "2x", "1/8x", "1/4x"};
  return gain_str[gain & 0b11];
}

void VEML7700Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up VEML7700/6030...");

  auto err = this->configure_();
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Sensor configuration failed");
    this->mark_failed();
  } else {
    this->state_ = State::INITIAL_SETUP_COMPLETED;
  }
}

void VEML7700Component::dump_config() {
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Automatic gain/time: %s", YESNO(this->automatic_mode_enabled_));
  if (!this->automatic_mode_enabled_) {
    ESP_LOGCONFIG(TAG, "  Gain: %s", get_gain_str(this->gain_));
    ESP_LOGCONFIG(TAG, "  Integration time: %d ms", get_itime_ms(this->integration_time_));
  }
  ESP_LOGCONFIG(TAG, "  Lux compensation: %s", YESNO(this->lux_compensation_enabled_));
  ESP_LOGCONFIG(TAG, "  Glass attenuation factor: %f", this->glass_attenuation_factor_);
  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "ALS channel lux", this->ambient_light_sensor_);
  LOG_SENSOR("  ", "ALS channel counts", this->ambient_light_counts_sensor_);
  LOG_SENSOR("  ", "WHITE channel lux", this->white_sensor_);
  LOG_SENSOR("  ", "WHITE channel counts", this->white_counts_sensor_);
  LOG_SENSOR("  ", "FAKE_IR channel lux", this->fake_infrared_sensor_);
  LOG_SENSOR("  ", "Actual gain", this->actual_gain_sensor_);
  LOG_SENSOR("  ", "Actual integration time", this->actual_integration_time_sensor_);

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with I2C VEML7700/6030 failed!");
  }
}

void VEML7700Component::update() {
  if (this->is_ready() && this->state_ == State::IDLE) {
    ESP_LOGV(TAG, "Update: Initiating new data collection");

    this->state_ = this->automatic_mode_enabled_ ? State::COLLECTING_DATA_AUTO : State::COLLECTING_DATA;

    this->readings_.als_counts = 0;
    this->readings_.white_counts = 0;
    this->readings_.actual_time = this->integration_time_;
    this->readings_.actual_gain = this->gain_;
    this->readings_.als_lux = 0;
    this->readings_.white_lux = 0;
    this->readings_.fake_infrared_lux = 0;
  } else {
    ESP_LOGV(TAG, "Update: Component not ready yet");
  }
}

void VEML7700Component::loop() {
  ErrorCode err = i2c::ERROR_OK;

  if (this->state_ == State::INITIAL_SETUP_COMPLETED) {
    // Datasheet: 2.5 ms before the first measurement is needed, allowing for the correct start of the signal processor
    // and oscillator.
    // Reality: wait for couple integration times to have first samples captured
    this->set_timeout(2 * this->integration_time_, [this]() { this->state_ = State::IDLE; });
  }

  if (this->is_ready()) {
    switch (this->state_) {
      case State::IDLE:
        // doing nothing, having best time
        break;

      case State::COLLECTING_DATA:
        err = this->read_sensor_output_(this->readings_);
        this->state_ = (err == i2c::ERROR_OK) ? State::DATA_COLLECTED : State::IDLE;
        break;

      case State::COLLECTING_DATA_AUTO:  // Automatic mode - we start here to reconfigure device first
      case State::DATA_COLLECTED:
        if (!this->are_adjustments_required_(this->readings_)) {
          this->state_ = State::READY_TO_PUBLISH_PART_1;
        } else {
          // if sensitivity adjustment needed -
          // shutdown device to change config and wait one integration time period
          this->state_ = State::ADJUSTMENT_IN_PROGRESS;
          err = this->reconfigure_time_and_gain_(this->readings_.actual_time, this->readings_.actual_gain, true);
          if (err == i2c::ERROR_OK) {
            this->set_timeout(1 * get_itime_ms(this->readings_.actual_time),
                              [this]() { this->state_ = State::READY_TO_APPLY_ADJUSTMENTS; });
          } else {
            this->state_ = State::IDLE;
          }
        }
        break;

      case State::ADJUSTMENT_IN_PROGRESS:
        // nothing to be done, just waiting for the timeout
        break;

      case State::READY_TO_APPLY_ADJUSTMENTS:
        // second stage of sensitivity adjustment - turn device back on
        // and wait 2-3 integration time periods to get good data samples
        this->state_ = State::ADJUSTMENT_IN_PROGRESS;
        err = this->reconfigure_time_and_gain_(this->readings_.actual_time, this->readings_.actual_gain, false);
        if (err == i2c::ERROR_OK) {
          this->set_timeout(3 * get_itime_ms(this->readings_.actual_time),
                            [this]() { this->state_ = State::COLLECTING_DATA; });
        } else {
          this->state_ = State::IDLE;
        }
        break;

      case State::READY_TO_PUBLISH_PART_1:
        this->status_clear_warning();

        this->apply_lux_calculation_(this->readings_);
        this->apply_lux_compensation_(this->readings_);
        this->apply_glass_attenuation_(this->readings_);

        this->publish_data_part_1_(this->readings_);
        this->state_ = State::READY_TO_PUBLISH_PART_2;
        break;

      case State::READY_TO_PUBLISH_PART_2:
        this->publish_data_part_2_(this->readings_);
        this->state_ = State::READY_TO_PUBLISH_PART_3;
        break;

      case State::READY_TO_PUBLISH_PART_3:
        this->publish_data_part_3_(this->readings_);
        this->state_ = State::IDLE;
        break;

      default:
        break;
    }
    if (err != i2c::ERROR_OK)
      this->status_set_warning();
  }
}

ErrorCode VEML7700Component::configure_() {
  ESP_LOGV(TAG, "Configure");

  ConfigurationRegister als_conf{0};
  als_conf.ALS_INT_EN = false;
  als_conf.ALS_PERS = Persistence::PERSISTENCE_1;
  als_conf.ALS_IT = this->integration_time_;
  als_conf.ALS_GAIN = this->gain_;

  als_conf.ALS_SD = true;
  ESP_LOGV(TAG, "Shutdown before config. ALS_CONF_0 to 0x%04X", als_conf.raw);
  auto err = this->write_register((uint8_t) CommandRegisters::ALS_CONF_0, als_conf.raw_bytes, VEML_REG_SIZE);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Failed to shutdown, I2C error %d", err);
    return err;
  }
  delay(3);

  als_conf.ALS_SD = false;
  ESP_LOGV(TAG, "Turning on. Setting ALS_CONF_0 to 0x%04X", als_conf.raw);
  err = this->write_register((uint8_t) CommandRegisters::ALS_CONF_0, als_conf.raw_bytes, VEML_REG_SIZE);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Failed to turn on, I2C error %d", err);
    return err;
  }

  PSMRegister psm{0};
  psm.PSM = PSMMode::PSM_MODE_1;
  psm.PSM_EN = false;
  ESP_LOGV(TAG, "Setting PSM to 0x%04X", psm.raw);
  err = this->write_register((uint8_t) CommandRegisters::PWR_SAVING, psm.raw_bytes, VEML_REG_SIZE);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Failed to set PSM, I2C error %d", err);
    return err;
  }

  return err;
}

ErrorCode VEML7700Component::reconfigure_time_and_gain_(IntegrationTime time, Gain gain, bool shutdown) {
  ESP_LOGV(TAG, "Reconfigure time and gain (%d ms, %s) %s", get_itime_ms(time), get_gain_str(gain),
           shutdown ? "Shutting down" : "Turning back on");

  ConfigurationRegister als_conf{0};
  als_conf.raw = 0;

  // We have to before changing parameters
  als_conf.ALS_SD = shutdown;
  als_conf.ALS_INT_EN = false;
  als_conf.ALS_PERS = Persistence::PERSISTENCE_1;
  als_conf.ALS_IT = time;
  als_conf.ALS_GAIN = gain;
  auto err = this->write_register((uint8_t) CommandRegisters::ALS_CONF_0, als_conf.raw_bytes, VEML_REG_SIZE);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "%s failed", shutdown ? "Shutdown" : "Turn on");
  }

  return err;
}

ErrorCode VEML7700Component::read_sensor_output_(Readings &data) {
  auto als_err =
      this->read_register((uint8_t) CommandRegisters::ALS, (uint8_t *) &data.als_counts, VEML_REG_SIZE, false);
  if (als_err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Error reading ALS register, err = %d", als_err);
  }
  auto white_err =
      this->read_register((uint8_t) CommandRegisters::WHITE, (uint8_t *) &data.white_counts, VEML_REG_SIZE, false);
  if (white_err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Error reading WHITE register, err = %d", white_err);
  }

  ConfigurationRegister conf{0};
  auto err =
      this->read_register((uint8_t) CommandRegisters::ALS_CONF_0, (uint8_t *) conf.raw_bytes, VEML_REG_SIZE, false);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Error reading ALS_CONF_0 register, err = %d", white_err);
  }
  data.actual_time = conf.ALS_IT;
  data.actual_gain = conf.ALS_GAIN;

  ESP_LOGV(TAG, "Data from sensors: ALS = %d, WHITE = %d, Gain = %s, Time = %d ms", data.als_counts, data.white_counts,
           get_gain_str(data.actual_gain), get_itime_ms(data.actual_time));
  return std::max(als_err, white_err);
}

bool VEML7700Component::are_adjustments_required_(Readings &data) {
  // skip first sample in auto mode -
  // we need to reconfigure device after last measurement
  if (this->state_ == State::COLLECTING_DATA_AUTO)
    return true;

  if (!this->automatic_mode_enabled_)
    return false;

  // Recommended thresholds as per datasheet
  static constexpr uint16_t LOW_INTENSITY_THRESHOLD = 100;
  static constexpr uint16_t HIGH_INTENSITY_THRESHOLD = 10000;

  static const IntegrationTime TIMES[INTEGRATION_TIMES_COUNT] = {INTEGRATION_TIME_25MS,  INTEGRATION_TIME_50MS,
                                                                 INTEGRATION_TIME_100MS, INTEGRATION_TIME_200MS,
                                                                 INTEGRATION_TIME_400MS, INTEGRATION_TIME_800MS};
  static const Gain GAINS[GAINS_COUNT] = {X_1_8, X_1_4, X_1, X_2};

  if (data.als_counts <= LOW_INTENSITY_THRESHOLD) {
    Gain next_gain = get_next(GAINS, data.actual_gain);
    if (next_gain != data.actual_gain) {
      data.actual_gain = next_gain;
      return true;
    }
    IntegrationTime next_time = get_next(TIMES, data.actual_time);
    if (next_time != data.actual_time) {
      data.actual_time = next_time;
      return true;
    }
  } else if (data.als_counts >= HIGH_INTENSITY_THRESHOLD) {
    Gain prev_gain = get_prev(GAINS, data.actual_gain);
    if (prev_gain != data.actual_gain) {
      data.actual_gain = prev_gain;
      return true;
    }
    IntegrationTime prev_time = get_prev(TIMES, data.actual_time);
    if (prev_time != data.actual_time) {
      data.actual_time = prev_time;
      return true;
    }
  }

  // Counts are either good (between thresholds)
  // or there is no room to change sensitivity anymore
  return false;
}

void VEML7700Component::apply_lux_calculation_(Readings &data) {
  static const float MAX_GAIN = 2.0f;
  static const float MAX_ITIME_MS = 800.0f;
  static const float MAX_LX_RESOLUTION = 0.0036f;
  float lux_resolution = (MAX_ITIME_MS / (float) get_itime_ms(data.actual_time)) *
                         (MAX_GAIN / get_gain_coeff(data.actual_gain)) * MAX_LX_RESOLUTION;
  ESP_LOGV(TAG, "Lux resolution for (%d, %s) = %.4f ", get_itime_ms(data.actual_time), get_gain_str(data.actual_gain),
           lux_resolution);

  data.als_lux = lux_resolution * (float) data.als_counts;
  data.white_lux = lux_resolution * (float) data.white_counts;
  data.fake_infrared_lux = reduce_to_zero(data.white_lux, data.als_lux);

  ESP_LOGV(TAG, "%s mode - ALS = %.1f lx, WHITE = %.1f lx, FAKE_IR = %.1f lx",
           this->automatic_mode_enabled_ ? "Automatic" : "Manual", data.als_lux, data.white_lux,
           data.fake_infrared_lux);
}

void VEML7700Component::apply_lux_compensation_(Readings &data) {
  if (!this->lux_compensation_enabled_)
    return;
  auto &local_data = data;
  // Always apply correction for G1/4 and G1/8
  // Other Gains G1 and G2 are not supposed to be used for lux > 1000,
  // corrections may help, but not a lot.
  //
  // "Illumination values higher than 1000 lx show non-linearity.
  // This non-linearity is the same for all sensors, so a compensation formula can be applied
  // if this light level is exceeded"
  auto compensate = [&local_data](float &lux) {
    auto calculate_high_lux_compensation = [](float lux_veml) -> float {
      return (((6.0135e-13 * lux_veml - 9.3924e-9) * lux_veml + 8.1488e-5) * lux_veml + 1.0023) * lux_veml;
    };

    if (lux > 1000.0f || local_data.actual_gain == Gain::X_1_8 || local_data.actual_gain == Gain::X_1_4) {
      lux = calculate_high_lux_compensation(lux);
    }
  };

  compensate(data.als_lux);
  compensate(data.white_lux);
  data.fake_infrared_lux = reduce_to_zero(data.white_lux, data.als_lux);

  ESP_LOGV(TAG, "Lux compensation - ALS = %.1f lx, WHITE = %.1f lx, FAKE_IR = %.1f lx", data.als_lux, data.white_lux,
           data.fake_infrared_lux);
}

void VEML7700Component::apply_glass_attenuation_(Readings &data) {
  data.als_lux *= this->glass_attenuation_factor_;
  data.white_lux *= this->glass_attenuation_factor_;
  data.fake_infrared_lux = reduce_to_zero(data.white_lux, data.als_lux);
  ESP_LOGV(TAG, "Glass attenuation - ALS = %.1f lx, WHITE = %.1f lx, FAKE_IR = %.1f lx", data.als_lux, data.white_lux,
           data.fake_infrared_lux);
}

void VEML7700Component::publish_data_part_1_(Readings &data) {
  if (this->ambient_light_sensor_ != nullptr) {
    this->ambient_light_sensor_->publish_state(data.als_lux);
  }
  if (this->white_sensor_ != nullptr) {
    this->white_sensor_->publish_state(data.white_lux);
  }
}

void VEML7700Component::publish_data_part_2_(Readings &data) {
  if (this->fake_infrared_sensor_ != nullptr) {
    this->fake_infrared_sensor_->publish_state(data.fake_infrared_lux);
  }
  if (this->ambient_light_counts_sensor_ != nullptr) {
    this->ambient_light_counts_sensor_->publish_state(data.als_counts);
  }
  if (this->white_counts_sensor_ != nullptr) {
    this->white_counts_sensor_->publish_state(data.white_counts);
  }
}

void VEML7700Component::publish_data_part_3_(Readings &data) {
  if (this->actual_gain_sensor_ != nullptr) {
    this->actual_gain_sensor_->publish_state(get_gain_coeff(data.actual_gain));
  }
  if (this->actual_integration_time_sensor_ != nullptr) {
    this->actual_integration_time_sensor_->publish_state(get_itime_ms(data.actual_time));
  }
}
}  // namespace veml7700
}  // namespace esphome
