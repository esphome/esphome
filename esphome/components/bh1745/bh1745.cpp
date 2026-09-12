#include "bh1745.h"
#include <cinttypes>
#include <cmath>
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::bh1745 {

static const char *const TAG = "bh1745";

static constexpr uint8_t BH1745_MANUFACTURER_ID = 0xE0;
static constexpr uint8_t BH1745_DEVICE_ID = 0b001011;
static constexpr uint8_t BH1745_RESET_TIMEOUT_MS = 100;
static constexpr uint8_t BH1745_BASE_MEAS_TIME_MS = 160;

static constexpr uint32_t BH1745_DATA_READY_GRACE_DIVIDER = 5;  // grace = integration time / 5

uint32_t get_measurement_time_ms(MeasurementTime time) {
  return ((uint32_t) BH1745_BASE_MEAS_TIME_MS) << static_cast<uint8_t>(time);
}

uint8_t get_adc_gain(AdcGain gain) {
  switch (gain) {
    case AdcGain::ADC_GAIN_1X:
      return 1;
    case AdcGain::ADC_GAIN_2X:
      return 2;
    case AdcGain::ADC_GAIN_16X:
      return 16;
    default:
      return 1;
  }
}

void BH1745Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up BH1745");

  uint8_t manuf_id = this->reg((uint8_t) BH1745Registers::MANUFACTURER_ID).get();
  if (manuf_id != BH1745_MANUFACTURER_ID) {
    ESP_LOGW(TAG, "Manufacturer ID of BH1745 is not correct! Got 0x%02X, expected 0x%02X", manuf_id,
             BH1745_MANUFACTURER_ID);
    this->mark_failed();
    return;
  }

  SystemControlRegister sys_ctrl;
  sys_ctrl.raw = this->reg((uint8_t) BH1745Registers::SYSTEM_CONTROL).get();
  if (sys_ctrl.part_id != BH1745_DEVICE_ID) {
    ESP_LOGW(TAG, "Device ID of BH1745 is not correct! Got 0x%02X, expected 0x%02X", sys_ctrl.part_id,
             BH1745_DEVICE_ID);
    this->mark_failed();
    return;
  }

  sys_ctrl.sw_reset = true;
  sys_ctrl.int_reset = false;
  this->reg((uint8_t) BH1745Registers::SYSTEM_CONTROL) = sys_ctrl.raw;

  // ESPHome doesnt support using interrupts yet, so set the threasholds to max/min
  // so that the int pin can be used as an open drain output if configured.
  static constexpr uint8_t TH_HIGH[2] = {0xFF, 0xFF};  // LSB first
  static constexpr uint8_t TH_LOW[2] = {0x00, 0x00};
  if (!this->write_bytes((uint8_t) BH1745Registers::TH_LSB, TH_HIGH, sizeof(TH_HIGH)) ||
      !this->write_bytes((uint8_t) BH1745Registers::TL_LSB, TH_LOW, sizeof(TH_LOW))) {
    ESP_LOGE(TAG, "Failed to write interrupt thresholds");
    this->mark_failed();
    return;
  }
  this->reg((uint8_t) BH1745Registers::INTERRUPT_REG) = 0x00;

  this->disable_loop();

  this->set_timeout(BH1745_RESET_TIMEOUT_MS, [this]() {
    this->configure_measurement_time_();
    this->state_ = State::INITIAL_SETUP_COMPLETED;
    this->enable_loop();
  });
}

void BH1745Component::dump_config() {
  ESP_LOGCONFIG(TAG, "BH1745:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);

  ESP_LOGCONFIG(TAG,
                "  Gain: %ux\n"
                "  Integration time: %" PRIu32 " ms\n"
                "  Glass attenuation factor: %f",
                get_adc_gain(this->adc_gain_), get_measurement_time_ms(this->measurement_time_),
                this->glass_attenuation_factor_);

  LOG_SENSOR("  ", "Red Counts", this->red_counts_sensor_);
  LOG_SENSOR("  ", "Green Counts", this->green_counts_sensor_);
  LOG_SENSOR("  ", "Blue Counts", this->blue_counts_sensor_);
  LOG_SENSOR("  ", "Clear Counts", this->clear_counts_sensor_);
  LOG_SENSOR("  ", "Illuminance", this->illuminance_sensor_);
  LOG_SENSOR("  ", "Color Temperature", this->color_temperature_sensor_);
}

void BH1745Component::update() {
  if (this->state_ == State::IDLE) {
    ESP_LOGV(TAG, "Initiating new data collection");

    this->state_ = State::MEASUREMENT_IN_PROGRESS;

    this->readings_.red = 0;
    this->readings_.green = 0;
    this->readings_.blue = 0;
    this->readings_.clear = 0;

    ModeControl2Register mode_ctrl2{0};
    mode_ctrl2.adc_gain = this->adc_gain_;
    mode_ctrl2.rgbc_measurement_enable = true;

    this->reg((uint8_t) BH1745Registers::MODE_CONTROL2) = mode_ctrl2.raw;

    uint32_t measurement_time = get_measurement_time_ms(this->measurement_time_);
    this->set_timeout(measurement_time, [this, measurement_time]() {
      this->data_ready_deadline_ms_ = millis() + measurement_time / BH1745_DATA_READY_GRACE_DIVIDER;
      this->state_ = State::WAITING_FOR_DATA;
      this->enable_loop();
    });
  }
}

void BH1745Component::loop() {
  switch (this->state_) {
    case State::INITIAL_SETUP_COMPLETED:
      this->state_ = State::DELAYED_SETUP;
      this->configure_gain_();
      this->reg((uint8_t) BH1745Registers::MODE_CONTROL3) = 0x02;
      this->disable_loop();
      this->set_timeout(BH1745_RESET_TIMEOUT_MS, [this]() { this->state_ = State::IDLE; });
      break;

    case State::WAITING_FOR_DATA:
      if (this->is_data_ready_(this->readings_)) {
        if (!this->read_data_(this->readings_)) {
          ESP_LOGW(TAG, "Failed to read measurement data. Aborting.");
          this->status_set_warning();
          this->state_ = State::IDLE;
          this->disable_loop();
          return;
        }
        this->state_ = State::DATA_COLLECTED;
        return;
      } else if ((int32_t) (App.get_loop_component_start_time() - this->data_ready_deadline_ms_) >= 0) {
        ESP_LOGW(TAG, "Data not ready in time. Aborting.");
        this->status_set_warning();
        this->state_ = State::IDLE;
        this->disable_loop();
        return;
      }
      break;

    case State::DATA_COLLECTED:
      this->status_clear_warning();
      this->publish_data_();
      this->state_ = State::IDLE;
      [[fallthrough]];
    case State::NOT_INITIALIZED:
    case State::DELAYED_SETUP:
    case State::IDLE:
    case State::MEASUREMENT_IN_PROGRESS:
    default:
      this->disable_loop();
      break;
  }
}

void BH1745Component::set_interrupt_state(bool on_off) {
  uint8_t raw = this->reg((uint8_t) BH1745Registers::INTERRUPT_REG).get();

  if (on_off) {
    raw |= (1);
  } else {
    raw &= ~(1);
  }
  this->reg((uint8_t) BH1745Registers::INTERRUPT_REG) = raw;
}

void BH1745Component::configure_measurement_time_() {
  ModeControl1Register mode_ctrl1;
  mode_ctrl1.reserved_3_7 = 0;
  mode_ctrl1.measurement_time = this->measurement_time_;
  this->reg((uint8_t) BH1745Registers::MODE_CONTROL1) = mode_ctrl1.raw;
}

void BH1745Component::configure_gain_() {
  ModeControl2Register mode_ctrl2;
  mode_ctrl2.raw = this->reg((uint8_t) BH1745Registers::MODE_CONTROL2).get();
  mode_ctrl2.adc_gain = this->adc_gain_;
  this->reg((uint8_t) BH1745Registers::MODE_CONTROL2) = mode_ctrl2.raw;
}

bool BH1745Component::is_data_ready_(Readings &data) {
  ModeControl2Register mode_ctrl2;
  mode_ctrl2.raw = this->reg((uint8_t) BH1745Registers::MODE_CONTROL2).get();
  if (mode_ctrl2.valid) {
    ModeControl1Register mode_ctrl1;
    mode_ctrl1.raw = this->reg((uint8_t) BH1745Registers::MODE_CONTROL1).get();

    data.meas_time = mode_ctrl1.measurement_time;
    data.gain = mode_ctrl2.adc_gain;
  }
  return mode_ctrl2.valid;
}

bool BH1745Component::read_data_(BH1745Component::Readings &data) {
  uint8_t buffer[BH1745_CHANNELS * 2];

  if (!this->read_bytes((uint8_t) BH1745Registers::RED_DATA_LSB, buffer, sizeof(buffer))) {
    return false;
  }
  data.red = ((uint16_t) buffer[1] << 8) | buffer[0];
  data.green = ((uint16_t) buffer[3] << 8) | buffer[2];
  data.blue = ((uint16_t) buffer[5] << 8) | buffer[4];
  data.clear = ((uint16_t) buffer[7] << 8) | buffer[6];

  ESP_LOGV(TAG, "Red: %u, Green: %u, Blue: %u, Clear: %u", data.red, data.green, data.blue, data.clear);
  return true;
}

float BH1745Component::calculate_lux_(const Readings &data) {
  float gain = get_adc_gain(data.gain);
  float integration_time = get_measurement_time_ms(data.meas_time);
  float lx_tmp;

  if (data.green < 1) {
    lx_tmp = 0.0f;
  } else if (((float) data.clear / (float) data.green) < 0.160f) {
    lx_tmp = 0.202f * data.red + 0.766f * data.green;
  } else {
    lx_tmp = 0.159f * data.red + 0.646f * data.green;
  }
  if (lx_tmp < 0.0f) {
    lx_tmp = 0.0f;
  }

  float lx = lx_tmp / gain / integration_time * 160.0f / this->glass_attenuation_factor_;
  ESP_LOGV(TAG, "Lux calculation: %.1f", lx);
  return lx;
}

float BH1745Component::calculate_cct_(const Readings &data) {
  uint32_t all = (uint32_t) data.red + data.green + data.blue;
  if (data.green < 1 || all < 1)
    return 0.0f;
  float r_ratio = (float) data.red / all;
  float b_ratio = (float) data.blue / all;
  float ct = 0.0f;
  if (((float) data.clear / (float) data.green) < 0.160f) {
    float b_eff = std::fmin(b_ratio * 3.13f, 1.0f);
    ct = ((1.0f - b_eff) * 12746.0f * std::exp(-2.911f * r_ratio)) + (b_eff * 1637.0f * std::exp(4.865f * b_ratio));
  } else {
    float b_eff = std::fmin(b_ratio * 10.67f, 1.0f);
    ct = ((1.0f - b_eff) * 16234.0f * std::exp(-2.781f * r_ratio)) + (b_eff * 1882.0f * std::exp(4.448f * b_ratio));
  }
  if (ct > 10000.0f) {
    ct = 10000.0f;
  }
  ESP_LOGV(TAG, "CCT calculation: %.1f", ct);
  return ct;
}

void BH1745Component::publish_data_() {
  if (this->red_counts_sensor_ != nullptr) {
    this->red_counts_sensor_->publish_state(this->readings_.red);
  }
  if (this->green_counts_sensor_ != nullptr) {
    this->green_counts_sensor_->publish_state(this->readings_.green);
  }
  if (this->blue_counts_sensor_ != nullptr) {
    this->blue_counts_sensor_->publish_state(this->readings_.blue);
  }
  if (this->clear_counts_sensor_ != nullptr) {
    this->clear_counts_sensor_->publish_state(this->readings_.clear);
  }
  if (this->illuminance_sensor_ != nullptr) {
    this->illuminance_sensor_->publish_state(this->calculate_lux_(this->readings_));
  }
  if (this->color_temperature_sensor_ != nullptr) {
    this->color_temperature_sensor_->publish_state(this->calculate_cct_(this->readings_));
  }
}

}  // namespace esphome::bh1745
