#include "ltr303.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

using esphome::i2c::ErrorCode;

namespace esphome {
namespace ltr303 {

static const char *const TAG = "ltr303";

static const uint8_t MAX_TRIES = 5;

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
  static const uint16_t ALS_INT_TIME[8] = {100, 50, 200, 400, 150, 250, 300, 350};
  return ALS_INT_TIME[time & 0b111];
}

static uint16_t get_meas_time_ms(MeasurementRepeatRate rate) {
  static const uint16_t ALS_MEAS_RATE[8] = {50, 100, 200, 500, 1000, 2000, 2000, 2000};
  return ALS_MEAS_RATE[rate & 0b111];
}

static float get_gain_coeff(Gain gain) {
  static const float ALS_GAIN[8] = {1, 2, 4, 8, 0, 0, 48, 96};
  return ALS_GAIN[gain & 0b111];
}

void LTR303Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up LTR-303/329");
  // As per datasheet we need to wait at least 100ms after power on to get ALS chip responsive
  this->set_timeout(100, [this]() { this->state_ = State::DELAYED_SETUP; });
}

void LTR303Component::dump_config() {
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Automatic mode: %s", ONOFF(this->automatic_mode_enabled_));
  ESP_LOGCONFIG(TAG, "  Gain: %.0fx", get_gain_coeff(this->gain_));
  ESP_LOGCONFIG(TAG, "  Integration time: %d ms", get_itime_ms(this->integration_time_));
  ESP_LOGCONFIG(TAG, "  Measurement repeat rate: %d ms", get_meas_time_ms(this->repeat_rate_));
  ESP_LOGCONFIG(TAG, "  Glass attenuation factor: %f", this->glass_attenuation_factor_);
  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "ALS calculated lux", this->ambient_light_sensor_);
  LOG_SENSOR("  ", "CH1 Infrared counts", this->infrared_counts_sensor_);
  LOG_SENSOR("  ", "CH0 Visible+IR counts", this->full_spectrum_counts_sensor_);
  LOG_SENSOR("  ", "Actual gain", this->actual_gain_sensor_);

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with I2C LTR-303/329 failed!");
  }
}

void LTR303Component::update() {
  ESP_LOGD(TAG, "Updating");
  if (this->is_ready() && this->state_ == State::IDLE) {
    ESP_LOGD(TAG, "Initiating new data collection");

    this->state_ = this->automatic_mode_enabled_ ? State::COLLECTING_DATA_AUTO : State::WAITING_FOR_DATA;

    this->readings_.ch0 = 0;
    this->readings_.ch1 = 0;
    this->readings_.actual_gain = this->gain_;
    this->readings_.integration_time = this->integration_time_;
    this->readings_.lux = 0;

  } else {
    ESP_LOGD(TAG, "Component not ready yet");
  }
}

void LTR303Component::loop() {
  ErrorCode err = i2c::ERROR_OK;
  static uint8_t tries{0};

  switch (this->state_) {
    case State::DELAYED_SETUP:
      this->configure_reset_and_activate_();
      this->configure_integration_time_(this->integration_time_);
      err = this->write(nullptr, 0);
      if (err != i2c::ERROR_OK) {
        ESP_LOGD(TAG, "i2c connection failed");
        this->mark_failed();
      }
      this->state_ = State::IDLE;
      break;

    case State::IDLE:
      // having fun, waiting for work
      break;

    case State::WAITING_FOR_DATA:
      if (this->is_data_ready_(this->readings_) == DataAvail::DATA_OK) {
        tries = 0;
        ESP_LOGD(TAG, "Reading sensor data having gain = %.0fx, time = %d ms",
                 get_gain_coeff(this->readings_.actual_gain), get_itime_ms(this->readings_.integration_time));
        this->read_sensor_data_(this->readings_);
        this->state_ = State::DATA_COLLECTED;
      } else if (tries >= MAX_TRIES) {
        ESP_LOGW(TAG, "Can't get data after several tries.");
        tries = 0;
        this->status_set_warning();
        this->state_ = State::IDLE;
        return;
      } else {
        tries++;
      }
      break;

    case State::COLLECTING_DATA_AUTO:
    case State::DATA_COLLECTED:
      if (this->state_ == State::COLLECTING_DATA_AUTO || this->are_adjustments_required_(this->readings_)) {
        this->state_ = State::ADJUSTMENT_IN_PROGRESS;
        ESP_LOGD(TAG, "Reconfiguring sensitivity");
        this->configure_integration_time_(this->readings_.integration_time);
        this->configure_gain_(this->readings_.actual_gain);
        // if sensitivity adjustment needed - need to wait for first data samples after setting new parameters
        this->set_timeout(1 * get_meas_time_ms(this->repeat_rate_),
                          [this]() { this->state_ = State::WAITING_FOR_DATA; });
      } else {
        this->apply_lux_calculation_(this->readings_);
        this->state_ = State::READY_TO_PUBLISH;
      }
      break;

    case State::ADJUSTMENT_IN_PROGRESS:
      // nothing to be done, just waiting for the timeout
      break;

    case State::READY_TO_PUBLISH:
      this->publish_data_part_1_(this->readings_);
      this->state_ = State::KEEP_PUBLISHING;
      break;

    case State::KEEP_PUBLISHING:
      this->publish_data_part_2_(this->readings_);
      this->status_clear_warning();
      this->state_ = State::IDLE;
      break;

    default:
      break;
  }
}
void LTR303Component::configure_reset_and_activate_() {
  ESP_LOGD(TAG, "Resetting");

  ControlRegister als_ctrl{0};
  als_ctrl.sw_reset = true;
  this->reg((uint8_t) CommandRegisters::ALS_CTRL) = als_ctrl.raw;
  delay(2);

  uint8_t tries = MAX_TRIES;
  do {
    ESP_LOGD(TAG, "Waiting chip to reset");
    delay(2);
    als_ctrl.raw = this->reg((uint8_t) CommandRegisters::ALS_CTRL).get();
  } while (als_ctrl.sw_reset && tries--);  // while sw reset bit is on - keep waiting

  if (als_ctrl.sw_reset) {
    ESP_LOGW(TAG, "Failed to finalize reset procedure");
  }

  als_ctrl.sw_reset = false;
  als_ctrl.active_mode = true;
  als_ctrl.gain = this->gain_;

  ESP_LOGD(TAG, "Setting active mode and gain reg 0x%02X", als_ctrl.raw);
  this->reg((uint8_t) CommandRegisters::ALS_CTRL) = als_ctrl.raw;
  delay(5);

  tries = MAX_TRIES;
  do {
    ESP_LOGD(TAG, "Waiting for device to become active...");
    delay(2);
    als_ctrl.raw = this->reg((uint8_t) CommandRegisters::ALS_CTRL).get();
  } while (!als_ctrl.active_mode && tries--);  // while active mode is not set - keep waiting

  if (!als_ctrl.active_mode) {
    ESP_LOGW(TAG, "Failed to activate device");
  }
}

void LTR303Component::configure_gain_(Gain gain) {
  ControlRegister als_ctrl{0};
  als_ctrl.active_mode = true;
  als_ctrl.gain = gain;
  this->reg((uint8_t) CommandRegisters::ALS_CTRL) = als_ctrl.raw;
  delay(2);
}

void LTR303Component::configure_integration_time_(IntegrationTime time) {
  MeasurementRateRegister meas{0};
  meas.measurement_repeat_rate = this->repeat_rate_;
  meas.integration_time = time;
  this->reg((uint8_t) CommandRegisters::MEAS_RATE) = meas.raw;
  delay(2);
}

DataAvail LTR303Component::is_data_ready_(Readings &data) {
  StatusRegister als_status{0};

  als_status.raw = this->reg((uint8_t) CommandRegisters::ALS_STATUS).get();
  if (!als_status.new_data)
    return DataAvail::NO_DATA;

  if (als_status.data_invalid) {
    ESP_LOGW(TAG, "Data available but not valid");
    return DataAvail::BAD_DATA;
  }

  // data.actual_gain = als_status.gain;
  ESP_LOGD(TAG, "Data ready, reported gain is %.0f", get_gain_coeff(als_status.gain));
  return DataAvail::DATA_OK;
}

void LTR303Component::read_sensor_data_(Readings &data) {
  uint8_t ch1_0 = this->reg((uint8_t) CommandRegisters::CH1_0).get();
  uint8_t ch1_1 = this->reg((uint8_t) CommandRegisters::CH1_1).get();
  uint8_t ch0_0 = this->reg((uint8_t) CommandRegisters::CH0_0).get();
  uint8_t ch0_1 = this->reg((uint8_t) CommandRegisters::CH0_1).get();
  data.ch1 = encode_uint16(ch1_1, ch1_0);
  data.ch0 = encode_uint16(ch0_1, ch0_0);

  ESP_LOGD(TAG, "Got sensor data: CH1 = %d, CH0 = %d", data.ch1, data.ch0);
}

bool LTR303Component::are_adjustments_required_(Readings &data) {
  // skip first sample in auto mode -
  // we need to reconfigure device after last measurement
  if (!this->automatic_mode_enabled_)
    return false;

  // Recommended thresholds as per datasheet
  static const uint16_t LOW_INTENSITY_THRESHOLD = 1000;
  static const uint16_t HIGH_INTENSITY_THRESHOLD = 20000;
  static const Gain GAINS[GAINS_COUNT] = {GAIN_1, GAIN_2, GAIN_4, GAIN_8, GAIN_48, GAIN_96};
  static const IntegrationTime INT_TIMES[TIMES_COUNT] = {
      INTEGRATION_TIME_50MS,  INTEGRATION_TIME_100MS, INTEGRATION_TIME_150MS, INTEGRATION_TIME_200MS,
      INTEGRATION_TIME_250MS, INTEGRATION_TIME_300MS, INTEGRATION_TIME_350MS, INTEGRATION_TIME_400MS};

  if (data.ch0 <= LOW_INTENSITY_THRESHOLD) {
    Gain next_gain = get_next(GAINS, data.actual_gain);
    if (next_gain != data.actual_gain) {
      data.actual_gain = next_gain;
      ESP_LOGD(TAG, "Low illuminance. Increasing gain.");
      return true;
    }
    IntegrationTime next_time = get_next(INT_TIMES, data.integration_time);
    if (next_time != data.integration_time) {
      data.integration_time = next_time;
      ESP_LOGD(TAG, "Low illuminance. Increasing integration time.");
      return true;
    }
  } else if (data.ch0 >= HIGH_INTENSITY_THRESHOLD) {
    Gain prev_gain = get_prev(GAINS, data.actual_gain);
    if (prev_gain != data.actual_gain) {
      data.actual_gain = prev_gain;
      ESP_LOGD(TAG, "High illuminance. Decreasing gain.");
      return true;
    }
    IntegrationTime prev_time = get_prev(INT_TIMES, data.integration_time);
    if (prev_time != data.integration_time) {
      data.integration_time = prev_time;
      ESP_LOGD(TAG, "High illuminance. Decreasing integration time.");
      return true;
    }
  } else {
    ESP_LOGD(TAG, "Illuminance is good enough.");
    return false;
  }
  ESP_LOGD(TAG, "Can't adjust sensitivity anymore.");
  return false;
}

void LTR303Component::apply_lux_calculation_(Readings &data) {
  if ((data.ch0 == 0xFFFF) || (data.ch1 == 0xFFFF)) {
    ESP_LOGW(TAG, "Sensors got saturated");
    data.lux = 0.0f;
    return;
  }

  if ((data.ch0 == 0x0000) && (data.ch1 == 0x0000)) {
    ESP_LOGW(TAG, "Sensors blacked out");
    data.lux = 0.0f;
    return;
  }

  float ch0 = data.ch0;
  float ch1 = data.ch1;
  float ratio = ch1 / (ch0 + ch1);
  float als_gain = get_gain_coeff(data.actual_gain);
  float als_time = ((float) get_itime_ms(data.integration_time)) / 100.0f;
  float inv_pfactor = this->glass_attenuation_factor_;
  float lux = 0.0f;

  ESP_LOGD(TAG, "Lux calculation: ratio %f, gain %f, int time %f, inv_pfactor %f", ratio, als_gain, als_time,
           inv_pfactor);

  if (ratio < 0.45) {
    lux = (1.7743 * ch0 + 1.1059 * ch1);
  } else if (ratio < 0.64 && ratio >= 0.45) {
    lux = (4.2785 * ch0 - 1.9548 * ch1);
  } else if (ratio < 0.85 && ratio >= 0.64) {
    lux = (0.5926 * ch0 + 0.1185 * ch1);
  } else {
    ESP_LOGW(TAG, "Impossible ch1/(ch0 + ch1) ratio");
    lux = 0.0f;
  }
  lux = inv_pfactor * lux / als_gain / als_time;
  data.lux = lux;
}

void LTR303Component::publish_data_part_1_(Readings &data) {
  if (this->ambient_light_sensor_ != nullptr) {
    this->ambient_light_sensor_->publish_state(data.lux);
  }
  if (this->infrared_counts_sensor_ != nullptr) {
    this->infrared_counts_sensor_->publish_state(data.ch1);
  }
  if (this->full_spectrum_counts_sensor_ != nullptr) {
    this->full_spectrum_counts_sensor_->publish_state(data.ch0);
  }
}

void LTR303Component::publish_data_part_2_(Readings &data) {
  if (this->actual_gain_sensor_ != nullptr) {
    this->actual_gain_sensor_->publish_state(get_gain_coeff(data.actual_gain));
  }
  if (this->actual_integration_time_sensor_ != nullptr) {
    this->actual_integration_time_sensor_->publish_state(get_itime_ms(data.integration_time));
  }
}
}  // namespace ltr303
}  // namespace esphome
