#include "ltr303.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace ltr303 {

static const char *const TAG = "ltr303";
static const int ALS_GAIN[8] = {1, 2, 4, 8, 0, 0, 48, 96};                        // multiplier
static const int ALS_INT_TIME[8] = {100, 50, 200, 400, 150, 250, 300, 350};       // ms
static const int ALS_MEAS_RATE[8] = {50, 100, 200, 500, 1000, 2000, 2000, 2000};  // ms

float LTR303Component::calculate_lux_(Readings &data) {
  if ((data.ch0 == 0xFFFF) || (data.ch1 == 0xFFFF)) {
    ESP_LOGW(TAG, "Sensors got saturated");
    return 0.0f;
  }

  if ((data.ch0 == 0x0000) && (data.ch1 == 0x0000)) {
    ESP_LOGW(TAG, "Sensors blacked out");
    return 0.0f;
  }

  float ch0 = data.ch0;
  float ch1 = data.ch1;
  float ratio = ch1 / (ch0 + ch1);
  float als_gain = ALS_GAIN[data.gain];
  float als_time = ((float) ALS_INT_TIME[data.integration_time]) / 100.0f;
  float inv_pfactor = this->attenuation_factor_;
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
  return lux;
}

bool LTR303Component::read_data_manual_(Readings &data) {
  if (!this->read_sensor_data_(data))
    return false;

  if (data.gain != this->gain_) {
    ESP_LOGW(TAG, "Actual gain (%d) differs from requested (%d)", ALS_GAIN[data.gain], ALS_GAIN[this->gain_]);
  }
  return true;
}

static const uint16_t LOW_THRESHOLD = 0.10 * 0xffff;
static const uint16_t HIGH_THRESHOLD = 0.75 * 0xffff;

bool LTR303Component::read_data_automatic_(Readings &data) {
  Gain gains[GAINS_COUNT] = {GAIN_1, GAIN_2, GAIN_4, GAIN_8, GAIN_48, GAIN_96};
  IntegrationTime times[TIMES_COUNT] = {INTEGRATION_TIME_50MS,  INTEGRATION_TIME_100MS, INTEGRATION_TIME_150MS,
                                        INTEGRATION_TIME_200MS, INTEGRATION_TIME_250MS, INTEGRATION_TIME_300MS,
                                        INTEGRATION_TIME_350MS, INTEGRATION_TIME_400MS};
  uint8_t gain_idx{0};
  uint8_t time_idx{0};

  bool readings_are_okay{false};
  static Gain current_gain = this->gain_;
  static IntegrationTime current_time = this->integration_time_;
  bool wait_needed = false;
  do {
    ESP_LOGD(TAG, "Auto - gain = %dx, time = %d ms", ALS_GAIN[gains[gain_idx]], ALS_INT_TIME[times[time_idx]]);

    if (current_time != times[time_idx]) {
      this->configure_integration_time_(times[time_idx]);
      current_time = times[time_idx];
      wait_needed = true;
    }

    if (current_gain != gains[gain_idx]) {
      this->configure_gain_(gains[gain_idx]);
      current_gain = gains[gain_idx];
      wait_needed = true;
    }
    // wait for new data samples to come
    // delay(ALS_INT_TIME[current_time] * 2);
    if (wait_needed) {
      delay(ALS_MEAS_RATE[this->repeat_rate_]);
      App.feed_wdt();
    }

    if (!this->read_sensor_data_(data))
      return false;

    if (data.ch0 < LOW_THRESHOLD) {
      // low ? time+ -> gain+
      if (time_idx < TIMES_COUNT - 1) {
        ESP_LOGD(TAG, "Low illuminance. Increasing int.time");
        time_idx++;
      } else if (gain_idx < GAINS_COUNT - 1) {
        ESP_LOGD(TAG, "Low illuminance. Increasing gain");
        gain_idx++;
      } else {
        ESP_LOGD(TAG, "Low illuminance. We will live with it");
        readings_are_okay = true;
      }
    } else if (data.ch0 > HIGH_THRESHOLD) {
      // high ? gain- -> time-
      if (gain_idx > 0) {
        ESP_LOGD(TAG, "High illuminance. Decreasing gain");
        gain_idx--;
      } else if (time_idx > 0) {
        ESP_LOGD(TAG, "High illuminance. Decreasing int.time");
        time_idx--;
      } else {
        ESP_LOGD(TAG, "High illuminance. We will live with it");
        readings_are_okay = true;
      }
    } else {
      ESP_LOGD(TAG, "Illiminance is good enough");
      readings_are_okay = true;
    }
  } while (!readings_are_okay);

  data.gain = gains[gain_idx];
  data.integration_time = times[time_idx];

  return true;
}

bool LTR303Component::read_sensor_data_(Readings &data) {
  if (!this->data_ready_(data))
    return false;

  uint8_t ch1_0 = this->reg(CommandRegisters::CR_CH1_0).get();
  uint8_t ch1_1 = this->reg(CommandRegisters::CR_CH1_1).get();
  uint8_t ch0_0 = this->reg(CommandRegisters::CR_CH0_0).get();
  uint8_t ch0_1 = this->reg(CommandRegisters::CR_CH0_1).get();
  data.ch1 = encode_uint16(ch1_1, ch1_0);
  data.ch0 = encode_uint16(ch0_1, ch0_0);

  ESP_LOGD(TAG, "Got sensor data: CH1 = %d, CH0 = %d", data.ch1, data.ch0);
  return true;
}

bool LTR303Component::data_ready_(Readings &data) {
  StatusRegister als_status{0};

  const uint32_t start = millis();
  while (true) {
    als_status.raw = this->reg(CommandRegisters::CR_ALS_STATUS).get();
    //    ESP_LOGD(TAG, "LTR303_ALS_STATUS = 0x%02X", als_status.raw);

    if (als_status.new_data) {
      //      ESP_LOGD(TAG, "New data is ready");
      break;
    }
    if (millis() - start > 100) {
      ESP_LOGW(TAG, "No data returned from sensor");
      return false;
    }
    ESP_LOGW(TAG, "Waiting for data");
    delay(5);
  }

  if (als_status.data_invalid) {
    ESP_LOGW(TAG, "Data available but not valid");
    return false;
  }
  data.gain = als_status.gain;
  return true;
}

bool LTR303Component::read_data_() {
  ESP_LOGD(TAG, "Reading sensor data");

  Readings data;
  data.gain = this->gain_;
  data.integration_time = this->integration_time_;
  bool ret = this->automatic_mode_enabled_ ? this->read_data_automatic_(data) : this->read_data_manual_(data);
  if (ret) {
    this->status_clear_warning();
  } else {
    this->status_set_warning();
  }

  if (this->infrared_counts_sensor_ != nullptr) {
    this->infrared_counts_sensor_->publish_state(data.ch1);
  }

  if (this->full_spectrum_counts_sensor_ != nullptr) {
    this->full_spectrum_counts_sensor_->publish_state(data.ch0);
  }

  if (this->actual_gain_sensor_ != nullptr) {
    this->actual_gain_sensor_->publish_state(ALS_GAIN[data.gain]);
  }

  if (this->actual_integration_time_sensor_ != nullptr) {
    this->actual_integration_time_sensor_->publish_state(ALS_INT_TIME[data.integration_time]);
  }

  if (this->ambient_light_sensor_ != nullptr) {
    float lux = this->calculate_lux_(data);
    ESP_LOGD(TAG, "Calculated ALS: %f lx", lux);
    this->ambient_light_sensor_->publish_state(lux);
  }

  return true;
}

void LTR303Component::reset_() {
  ESP_LOGD(TAG, "Resetting");

  ControlRegister als_ctrl{0};
  als_ctrl.sw_reset = true;
  this->reg(CommandRegisters::CR_ALS_CTRL) = als_ctrl.raw;
  delay(10);

  uint8_t tries = 10;
  do {
    ESP_LOGD(TAG, "Waiting chip to reset");
    delay(5);
    als_ctrl.raw = this->reg(CommandRegisters::CR_ALS_CTRL).get();
  } while (als_ctrl.sw_reset && tries--);  // while sw reset bit is on - keep waiting

  if (als_ctrl.sw_reset) {
    ESP_LOGW(TAG, "Failed to finalize reset procedure");
  }
}

void LTR303Component::activate_() {
  ESP_LOGD(TAG, "Activating");

  ControlRegister als_ctrl{0};
  als_ctrl.active_mode = true;
  als_ctrl.gain = this->gain_;

  ESP_LOGD(TAG, "Setting active mode and gain reg 0x%02X", als_ctrl.raw);
  this->reg(CommandRegisters::CR_ALS_CTRL) = als_ctrl.raw;
  delay(10);

  uint8_t tries = 10;
  do {
    ESP_LOGD(TAG, "Waiting for device to become active...");
    delay(5);
    als_ctrl.raw = this->reg(CommandRegisters::CR_ALS_CTRL).get();
  } while (!als_ctrl.active_mode && tries--);  // while active mode is not set - keep waiting

  if (!als_ctrl.active_mode) {
    ESP_LOGW(TAG, "Failed to activate device");
  }
}

void LTR303Component::configure_gain_(Gain gain) {
  ControlRegister als_ctrl{0};
  als_ctrl.active_mode = true;
  als_ctrl.gain = gain;
//  ESP_LOGD(TAG, "Setting gain reg to 0x%02X", als_ctrl.raw);
  this->reg(CommandRegisters::CR_ALS_CTRL) = als_ctrl.raw;
  delay(10);
}

void LTR303Component::configure_integration_time_(IntegrationTime time) {
  MeasurementRateRegister meas{0};
  meas.measurement_repeat_rate = this->repeat_rate_;
  meas.integration_time = time;
//  ESP_LOGD(TAG, "Setting integration time and measurement repeat rate reg to 0x%02X", meas.raw);
  this->reg(CommandRegisters::CR_MEAS_RATE) = meas.raw;
  delay(10);
}

void LTR303Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up LTR-303/329");

  // As per datasheet we need to wait at least 100ms after power on to get ALS chip responsive
  // It' only done once during setup phase
  delay(100);  // NOLINT
  App.feed_wdt();

  this->reset_();
  this->activate_();
  this->configure_integration_time_(this->integration_time_);

  auto err = this->write(nullptr, 0);
  if (err != i2c::ERROR_OK) {
    ESP_LOGD(TAG, "i2c connection failed");
    this->mark_failed();
    return;
  }
}

void LTR303Component::dump_config() {
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Gain: x%d", ALS_GAIN[this->gain_]);
  ESP_LOGCONFIG(TAG, "  Integration time: %d ms", ALS_INT_TIME[this->integration_time_]);
  ESP_LOGCONFIG(TAG, "  Measurement repeat rate: %d ms", ALS_MEAS_RATE[this->repeat_rate_]);
  ESP_LOGCONFIG(TAG, "  Attenuation factor: %f", this->attenuation_factor_);
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

  if (this->is_ready() && !this->reading_data_) {
    this->reading_data_ = true;
    this->read_data_();
    this->reading_data_ = false;
  }
}

}  // namespace ltr303
}  // namespace esphome
