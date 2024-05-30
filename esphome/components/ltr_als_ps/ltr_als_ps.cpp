#include "ltr_als_ps.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

using esphome::i2c::ErrorCode;

namespace esphome {
namespace ltr_als_ps {

static const char *const TAG = "ltr_als_ps";

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

static float get_gain_coeff(AlsGain gain) {
  static const float ALS_GAIN[8] = {1, 2, 4, 8, 0, 0, 48, 96};
  return ALS_GAIN[gain & 0b111];
}

static float get_ps_gain_coeff(PsGain gain) {
  static const float PS_GAIN[4] = {16, 0, 32, 64};
  return PS_GAIN[gain & 0b11];
}

void LTRAlsPsComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up LTR-303/329/55x/659");
  // As per datasheet we need to wait at least 100ms after power on to get ALS chip responsive
  this->set_timeout(100, [this]() { this->state_ = State::DELAYED_SETUP; });
}

void LTRAlsPsComponent::dump_config() {
  auto get_device_type = [](LtrType typ) {
    switch (typ) {
      case LtrType::LTR_TYPE_ALS_ONLY:
        return "ALS only";
      case LtrType::LTR_TYPE_PS_ONLY:
        return "PS only";
      case LtrType::LTR_TYPE_ALS_AND_PS:
        return "ALS + PS";
      default:
        return "Unknown";
    }
  };

  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Device type: %s", get_device_type(this->ltr_type_));
  if (this->is_als_()) {
    ESP_LOGCONFIG(TAG, "  Automatic mode: %s", ONOFF(this->automatic_mode_enabled_));
    ESP_LOGCONFIG(TAG, "  Gain: %.0fx", get_gain_coeff(this->gain_));
    ESP_LOGCONFIG(TAG, "  Integration time: %d ms", get_itime_ms(this->integration_time_));
    ESP_LOGCONFIG(TAG, "  Measurement repeat rate: %d ms", get_meas_time_ms(this->repeat_rate_));
    ESP_LOGCONFIG(TAG, "  Glass attenuation factor: %f", this->glass_attenuation_factor_);
    LOG_SENSOR("  ", "ALS calculated lux", this->ambient_light_sensor_);
    LOG_SENSOR("  ", "CH1 Infrared counts", this->infrared_counts_sensor_);
    LOG_SENSOR("  ", "CH0 Visible+IR counts", this->full_spectrum_counts_sensor_);
    LOG_SENSOR("  ", "Actual gain", this->actual_gain_sensor_);
  }
  if (this->is_ps_()) {
    ESP_LOGCONFIG(TAG, "  Proximity gain: %.0fx", get_ps_gain_coeff(this->ps_gain_));
    ESP_LOGCONFIG(TAG, "  Proximity cooldown time: %d s", this->ps_cooldown_time_s_);
    ESP_LOGCONFIG(TAG, "  Proximity high threshold: %d", this->ps_threshold_high_);
    ESP_LOGCONFIG(TAG, "  Proximity low threshold: %d", this->ps_threshold_low_);
    LOG_SENSOR("  ", "Proximity counts", this->proximity_counts_sensor_);
  }
  LOG_UPDATE_INTERVAL(this);

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with I2C LTR-303/329/55x/659 failed!");
  }
}

void LTRAlsPsComponent::update() {
  ESP_LOGV(TAG, "Updating");
  if (this->is_ready() && this->state_ == State::IDLE) {
    ESP_LOGV(TAG, "Initiating new data collection");

    this->state_ = this->automatic_mode_enabled_ ? State::COLLECTING_DATA_AUTO : State::WAITING_FOR_DATA;

    this->als_readings_.ch0 = 0;
    this->als_readings_.ch1 = 0;
    this->als_readings_.gain = this->gain_;
    this->als_readings_.integration_time = this->integration_time_;
    this->als_readings_.lux = 0;
    this->als_readings_.number_of_adjustments = 0;

  } else {
    ESP_LOGV(TAG, "Component not ready yet");
  }
}

void LTRAlsPsComponent::loop() {
  ErrorCode err = i2c::ERROR_OK;
  static uint8_t tries{0};

  switch (this->state_) {
    case State::DELAYED_SETUP:
      err = this->write(nullptr, 0);
      if (err != i2c::ERROR_OK) {
        ESP_LOGV(TAG, "i2c connection failed");
        this->mark_failed();
      }
      this->configure_reset_();
      if (this->is_als_()) {
        this->configure_als_();
        this->configure_integration_time_(this->integration_time_);
      }
      if (this->is_ps_()) {
        this->configure_ps_();
      }

      this->state_ = State::IDLE;
      break;

    case State::IDLE:
      if (this->is_ps_()) {
        check_and_trigger_ps_();
      }
      break;

    case State::WAITING_FOR_DATA:
      if (this->is_als_data_ready_(this->als_readings_) == DataAvail::DATA_OK) {
        tries = 0;
        ESP_LOGV(TAG, "Reading sensor data having gain = %.0fx, time = %d ms", get_gain_coeff(this->als_readings_.gain),
                 get_itime_ms(this->als_readings_.integration_time));
        this->read_sensor_data_(this->als_readings_);
        this->state_ = State::DATA_COLLECTED;
        this->apply_lux_calculation_(this->als_readings_);
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
      // first measurement in auto mode (COLLECTING_DATA_AUTO state) require device reconfiguration
      if (this->state_ == State::COLLECTING_DATA_AUTO || this->are_adjustments_required_(this->als_readings_)) {
        this->state_ = State::ADJUSTMENT_IN_PROGRESS;
        ESP_LOGD(TAG, "Reconfiguring sensitivity: gain = %.0fx, time = %d ms", get_gain_coeff(this->als_readings_.gain),
                 get_itime_ms(this->als_readings_.integration_time));
        this->configure_integration_time_(this->als_readings_.integration_time);
        this->configure_gain_(this->als_readings_.gain);
        // if sensitivity adjustment needed - need to wait for first data samples after setting new parameters
        this->set_timeout(2 * get_meas_time_ms(this->repeat_rate_),
                          [this]() { this->state_ = State::WAITING_FOR_DATA; });
      } else {
        this->state_ = State::READY_TO_PUBLISH;
      }
      break;

    case State::ADJUSTMENT_IN_PROGRESS:
      // nothing to be done, just waiting for the timeout
      break;

    case State::READY_TO_PUBLISH:
      this->publish_data_part_1_(this->als_readings_);
      this->state_ = State::KEEP_PUBLISHING;
      break;

    case State::KEEP_PUBLISHING:
      this->publish_data_part_2_(this->als_readings_);
      this->status_clear_warning();
      this->state_ = State::IDLE;
      break;

    default:
      break;
  }
}

void LTRAlsPsComponent::check_and_trigger_ps_() {
  static uint32_t last_high_trigger_time{0};
  static uint32_t last_low_trigger_time{0};
  uint16_t ps_data = this->read_ps_data_();
  uint32_t now = millis();

  if (ps_data != this->ps_readings_) {
    this->ps_readings_ = ps_data;
    // Higher values - object is closer to sensor
    if (ps_data > this->ps_threshold_high_ && now - last_high_trigger_time >= this->ps_cooldown_time_s_ * 1000) {
      last_high_trigger_time = now;
      ESP_LOGV(TAG, "Proximity high threshold triggered. Value = %d, Trigger level = %d", ps_data,
               this->ps_threshold_high_);
      this->on_ps_high_trigger_callback_.call();
    } else if (ps_data < this->ps_threshold_low_ && now - last_low_trigger_time >= this->ps_cooldown_time_s_ * 1000) {
      last_low_trigger_time = now;
      ESP_LOGV(TAG, "Proximity low threshold triggered. Value = %d, Trigger level = %d", ps_data,
               this->ps_threshold_low_);
      this->on_ps_low_trigger_callback_.call();
    }
  }
}

bool LTRAlsPsComponent::check_part_number_() {
  uint8_t manuf_id = this->reg((uint8_t) CommandRegisters::MANUFAC_ID).get();
  if (manuf_id != 0x05) {  // 0x05 is Lite-On Semiconductor Corp. ID
    ESP_LOGW(TAG, "Unknown manufacturer ID: 0x%02X", manuf_id);
    this->mark_failed();
    return false;
  }

  // Things getting not really funny here, we can't identify device type by part number ID
  // ======================== ========= ===== =================
  // Device                    Part ID   Rev   Capabilities
  // ======================== ========= ===== =================
  // Ltr-329/ltr-303            0x0a    0x00  Als 16b
  // Ltr-553/ltr-556/ltr-556    0x09    0x02  Als 16b + Ps 11b  diff nm sens
  // Ltr-659                    0x09    0x02  Ps 11b and ps gain
  //
  // There are other devices which might potentially work with default settings,
  // but registers layout is different and we can't use them properly. For ex. ltr-558

  PartIdRegister part_id{0};
  part_id.raw = this->reg((uint8_t) CommandRegisters::PART_ID).get();
  if (part_id.part_number_id != 0x0a && part_id.part_number_id != 0x09) {
    ESP_LOGW(TAG, "Unknown part number ID: 0x%02X. It might not work properly.", part_id.part_number_id);
    this->status_set_warning();
    return true;
  }
  return true;
}

void LTRAlsPsComponent::configure_reset_() {
  ESP_LOGV(TAG, "Resetting");

  AlsControlRegister als_ctrl{0};
  als_ctrl.sw_reset = true;
  this->reg((uint8_t) CommandRegisters::ALS_CONTR) = als_ctrl.raw;
  delay(2);

  uint8_t tries = MAX_TRIES;
  do {
    ESP_LOGV(TAG, "Waiting for chip to reset");
    delay(2);
    als_ctrl.raw = this->reg((uint8_t) CommandRegisters::ALS_CONTR).get();
  } while (als_ctrl.sw_reset && tries--);  // while sw reset bit is on - keep waiting

  if (als_ctrl.sw_reset) {
    ESP_LOGW(TAG, "Reset timed out");
  }
}

void LTRAlsPsComponent::configure_als_() {
  AlsControlRegister als_ctrl{0};

  als_ctrl.sw_reset = false;
  als_ctrl.active_mode = true;
  als_ctrl.gain = this->gain_;

  ESP_LOGV(TAG, "Setting active mode and gain reg 0x%02X", als_ctrl.raw);
  this->reg((uint8_t) CommandRegisters::ALS_CONTR) = als_ctrl.raw;
  delay(5);

  uint8_t tries = MAX_TRIES;
  do {
    ESP_LOGV(TAG, "Waiting for device to become active...");
    delay(2);
    als_ctrl.raw = this->reg((uint8_t) CommandRegisters::ALS_CONTR).get();
  } while (!als_ctrl.active_mode && tries--);  // while active mode is not set - keep waiting

  if (!als_ctrl.active_mode) {
    ESP_LOGW(TAG, "Failed to activate device");
  }
}

void LTRAlsPsComponent::configure_ps_() {
  PsMeasurementRateRegister ps_meas{0};
  ps_meas.ps_measurement_rate = PsMeasurementRate::PS_MEAS_RATE_50MS;
  this->reg((uint8_t) CommandRegisters::PS_MEAS_RATE) = ps_meas.raw;

  PsControlRegister ps_ctrl{0};
  ps_ctrl.ps_mode_active = true;
  ps_ctrl.ps_mode_xxx = true;
  this->reg((uint8_t) CommandRegisters::PS_CONTR) = ps_ctrl.raw;
}

uint16_t LTRAlsPsComponent::read_ps_data_() {
  AlsPsStatusRegister als_status{0};
  als_status.raw = this->reg((uint8_t) CommandRegisters::ALS_PS_STATUS).get();
  if (!als_status.ps_new_data || als_status.data_invalid) {
    return this->ps_readings_;
  }

  uint8_t ps_low = this->reg((uint8_t) CommandRegisters::PS_DATA_0).get();
  PsData1Register ps_high;
  ps_high.raw = this->reg((uint8_t) CommandRegisters::PS_DATA_1).get();

  uint16_t val = encode_uint16(ps_high.ps_data_high, ps_low);
  if (ps_high.ps_saturation_flag) {
    return 0x7ff;  // full 11 bit range
  }
  return val;
}

void LTRAlsPsComponent::configure_gain_(AlsGain gain) {
  AlsControlRegister als_ctrl{0};
  als_ctrl.active_mode = true;
  als_ctrl.gain = gain;
  this->reg((uint8_t) CommandRegisters::ALS_CONTR) = als_ctrl.raw;
  delay(2);

  AlsControlRegister read_als_ctrl{0};
  read_als_ctrl.raw = this->reg((uint8_t) CommandRegisters::ALS_CONTR).get();
  if (read_als_ctrl.gain != gain) {
    ESP_LOGW(TAG, "Failed to set gain. We will try one more time.");
    this->reg((uint8_t) CommandRegisters::ALS_CONTR) = als_ctrl.raw;
    delay(2);
  }
}

void LTRAlsPsComponent::configure_integration_time_(IntegrationTime time) {
  MeasurementRateRegister meas{0};
  meas.measurement_repeat_rate = this->repeat_rate_;
  meas.integration_time = time;
  this->reg((uint8_t) CommandRegisters::MEAS_RATE) = meas.raw;
  delay(2);

  MeasurementRateRegister read_meas{0};
  read_meas.raw = this->reg((uint8_t) CommandRegisters::MEAS_RATE).get();
  if (read_meas.integration_time != time) {
    ESP_LOGW(TAG, "Failed to set integration time. We will try one more time.");
    this->reg((uint8_t) CommandRegisters::MEAS_RATE) = meas.raw;
    delay(2);
  }
}

DataAvail LTRAlsPsComponent::is_als_data_ready_(AlsReadings &data) {
  AlsPsStatusRegister als_status{0};

  als_status.raw = this->reg((uint8_t) CommandRegisters::ALS_PS_STATUS).get();
  if (!als_status.als_new_data)
    return DataAvail::NO_DATA;

  if (als_status.data_invalid) {
    ESP_LOGW(TAG, "Data available but not valid");
    return DataAvail::BAD_DATA;
  }
  ESP_LOGV(TAG, "Data ready, reported gain is %.0f", get_gain_coeff(als_status.gain));
  if (data.gain != als_status.gain) {
    ESP_LOGW(TAG, "Actual gain differs from requested (%.0f)", get_gain_coeff(data.gain));
    return DataAvail::BAD_DATA;
  }
  return DataAvail::DATA_OK;
}

void LTRAlsPsComponent::read_sensor_data_(AlsReadings &data) {
  data.ch1 = 0;
  data.ch0 = 0;
  uint8_t ch1_0 = this->reg((uint8_t) CommandRegisters::ALS_DATA_CH1_0).get();
  uint8_t ch1_1 = this->reg((uint8_t) CommandRegisters::ALS_DATA_CH1_1).get();
  uint8_t ch0_0 = this->reg((uint8_t) CommandRegisters::ALS_DATA_CH0_0).get();
  uint8_t ch0_1 = this->reg((uint8_t) CommandRegisters::ALS_DATA_CH0_1).get();
  data.ch1 = encode_uint16(ch1_1, ch1_0);
  data.ch0 = encode_uint16(ch0_1, ch0_0);

  ESP_LOGV(TAG, "Got sensor data: CH1 = %d, CH0 = %d", data.ch1, data.ch0);
}

bool LTRAlsPsComponent::are_adjustments_required_(AlsReadings &data) {
  if (!this->automatic_mode_enabled_)
    return false;

  if (data.number_of_adjustments > 15) {
    // sometimes sensors fail to change sensitivity. this prevents us from infinite loop
    ESP_LOGW(TAG, "Too many sensitivity adjustments done. Apparently, sensor reconfiguration fails. Stopping.");
    return false;
  }
  data.number_of_adjustments++;

  // Recommended thresholds as per datasheet
  static const uint16_t LOW_INTENSITY_THRESHOLD = 1000;
  static const uint16_t HIGH_INTENSITY_THRESHOLD = 30000;
  static const AlsGain GAINS[GAINS_COUNT] = {GAIN_1, GAIN_2, GAIN_4, GAIN_8, GAIN_48, GAIN_96};
  static const IntegrationTime INT_TIMES[TIMES_COUNT] = {
      INTEGRATION_TIME_50MS,  INTEGRATION_TIME_100MS, INTEGRATION_TIME_150MS, INTEGRATION_TIME_200MS,
      INTEGRATION_TIME_250MS, INTEGRATION_TIME_300MS, INTEGRATION_TIME_350MS, INTEGRATION_TIME_400MS};

  if (data.ch0 <= LOW_INTENSITY_THRESHOLD) {
    AlsGain next_gain = get_next(GAINS, data.gain);
    if (next_gain != data.gain) {
      data.gain = next_gain;
      ESP_LOGV(TAG, "Low illuminance. Increasing gain.");
      return true;
    }
    IntegrationTime next_time = get_next(INT_TIMES, data.integration_time);
    if (next_time != data.integration_time) {
      data.integration_time = next_time;
      ESP_LOGV(TAG, "Low illuminance. Increasing integration time.");
      return true;
    }
  } else if (data.ch0 >= HIGH_INTENSITY_THRESHOLD) {
    AlsGain prev_gain = get_prev(GAINS, data.gain);
    if (prev_gain != data.gain) {
      data.gain = prev_gain;
      ESP_LOGV(TAG, "High illuminance. Decreasing gain.");
      return true;
    }
    IntegrationTime prev_time = get_prev(INT_TIMES, data.integration_time);
    if (prev_time != data.integration_time) {
      data.integration_time = prev_time;
      ESP_LOGV(TAG, "High illuminance. Decreasing integration time.");
      return true;
    }
  } else {
    ESP_LOGD(TAG, "Illuminance is sufficient.");
    return false;
  }
  ESP_LOGD(TAG, "Can't adjust sensitivity anymore.");
  return false;
}

void LTRAlsPsComponent::apply_lux_calculation_(AlsReadings &data) {
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
  float als_gain = get_gain_coeff(data.gain);
  float als_time = ((float) get_itime_ms(data.integration_time)) / 100.0f;
  float inv_pfactor = this->glass_attenuation_factor_;
  float lux = 0.0f;

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

  ESP_LOGV(TAG, "Lux calculation: ratio %.3f, gain %.0fx, int time %.1f, inv_pfactor %.3f, lux %.3f", ratio, als_gain,
           als_time, inv_pfactor, lux);
}

void LTRAlsPsComponent::publish_data_part_1_(AlsReadings &data) {
  if (this->proximity_counts_sensor_ != nullptr) {
    this->proximity_counts_sensor_->publish_state(this->ps_readings_);
  }
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

void LTRAlsPsComponent::publish_data_part_2_(AlsReadings &data) {
  if (this->actual_gain_sensor_ != nullptr) {
    this->actual_gain_sensor_->publish_state(get_gain_coeff(data.gain));
  }
  if (this->actual_integration_time_sensor_ != nullptr) {
    this->actual_integration_time_sensor_->publish_state(get_itime_ms(data.integration_time));
  }
}
}  // namespace ltr_als_ps
}  // namespace esphome
