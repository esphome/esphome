#include "bh1745.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <cmath>

namespace esphome {
namespace bh1745 {

static const char *const TAG = "bh1745";

static constexpr uint8_t BH1745_MANUFACTURER_ID = 0xE0;
static constexpr uint8_t BH1745_DEVICE_ID = 0b001011;
static constexpr uint8_t BH1745_RESET_TIMEOUT_MS = 100;
static constexpr uint8_t BH1745_BASE_MEAS_TIME_MS = 160;
static constexpr uint8_t BH1745_MAX_TRIES = 15;

static constexpr float CHANNEL_COMPENSATION[BH1745_CHANNELS] = {2.2f, 1.0f, 1.8f, 10.0f};

uint32_t get_measurement_time_ms(MeasurementTime time) {
  return ((uint32_t) BH1745_BASE_MEAS_TIME_MS) << static_cast<uint8_t>(time);
}

uint8_t get_adc_gain(AdcGain gain) {
  switch (gain) {
    case AdcGain::GAIN_1X:
      return 1;
    case AdcGain::GAIN_2X:
      return 2;
    case AdcGain::GAIN_16X:
      return 16;
    default:
      return 1;
  }
}

void BH1745Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up BH1745");

  uint8_t manuf_id = this->reg((uint8_t) Bh1745Registers::MANUFACTURER_ID).get();
  if (manuf_id != BH1745_MANUFACTURER_ID) {
    ESP_LOGW(TAG, "Manufacturer ID of BH1745 is not correct! Got 0x%02X, expected 0x%02X", manuf_id,
             BH1745_MANUFACTURER_ID);
    this->mark_failed();
    return;
  }

  SystemControlRegister sys_ctrl;
  sys_ctrl.raw = this->reg((uint8_t) Bh1745Registers::SYSTEM_CONTROL).get();
  if (sys_ctrl.part_id != BH1745_DEVICE_ID) {
    ESP_LOGW(TAG, "Device ID of BH1745 is not correct! Got 0x%02X, expected 0x%02X", sys_ctrl.part_id,
             BH1745_DEVICE_ID);
    this->mark_failed();
    return;
  }

  sys_ctrl.sw_reset = true;
  sys_ctrl.int_reset = false;
  this->reg((uint8_t) Bh1745Registers::SYSTEM_CONTROL) = sys_ctrl.raw;

  // only for pimoroni boards for now - there are LEDs connected to interrupt pin
  uint16_t TH_HIGH[1] = {0xFFFF};
  uint16_t TH_LOW[1] = {0x0000};
  this->write_bytes_16((uint8_t) Bh1745Registers::TH_LSB, TH_LOW, 1);
  this->write_bytes_16((uint8_t) Bh1745Registers::TL_LSB, TH_HIGH, 1);
  this->reg((uint8_t) Bh1745Registers::INTERRUPT_) = 0x00;

  this->set_timeout(BH1745_RESET_TIMEOUT_MS, [this]() {
    this->configure_measurement_time_();
    this->state_ = State::DELAYED_SETUP;
  });
}

void BH1745Component::dump_config() {
  ESP_LOGCONFIG(TAG, "BH1745:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with BH1745 failed!");
  }

  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "Red Counts", this->red_counts_sensor_);
  LOG_SENSOR("  ", "Green Counts", this->green_counts_sensor_);
  LOG_SENSOR("  ", "Blue Counts", this->blue_counts_sensor_);
  LOG_SENSOR("  ", "Clear Counts", this->clear_counts_sensor_);
}

void BH1745Component::update() {
  if (this->is_ready() && this->state_ == State::IDLE) {
    ESP_LOGV(TAG, "Initiating new data collection");

    this->state_ = State::MEASUREMENT_IN_PROGRESS;

    this->readings_.red = 0;
    this->readings_.green = 0;
    this->readings_.blue = 0;
    this->readings_.clear = 0;
    this->readings_.tries = 0;

    ModeControl2Register mode_ctrl2{0};
    mode_ctrl2.adc_gain = this->adc_gain_;
    mode_ctrl2.rgbc_measurement_enable = true;

    this->reg((uint8_t) Bh1745Registers::MODE_CONTROL2) = mode_ctrl2.raw;

    this->set_timeout(get_measurement_time_ms(this->measurement_time_),
                      [this]() { this->state_ = State::WAITING_FOR_DATA; });
  }
}

void BH1745Component::loop() {
  switch (this->state_) {
    case State::NOT_INITIALIZED:
      break;

    case State::DELAYED_SETUP:
      this->configure_gain_();
      this->reg((uint8_t) Bh1745Registers::MODE_CONTROL3) = 0x02;
      this->set_timeout(BH1745_RESET_TIMEOUT_MS, [this]() { this->state_ = State::IDLE; });
      break;

    case State::IDLE:
      break;

    case State::MEASUREMENT_IN_PROGRESS:
      break;

    case State::WAITING_FOR_DATA:
      if (this->is_data_ready_(this->readings_)) {
        this->read_data_(this->readings_);
        this->state_ = State::DATA_COLLECTED;
        return;
      } else if (this->readings_.tries > BH1745_MAX_TRIES) {
        ESP_LOGW(TAG, "Can't get data after several tries. Aborting.");
        this->status_set_warning();
        this->state_ = State::IDLE;
        return;
      } else {
        this->readings_.tries++;
      }
      break;

    case State::DATA_COLLECTED:
      this->publish_data_();
      this->state_ = State::IDLE;
      break;

    default:
      // wrong state
      break;
  }
}

float BH1745Component::get_setup_priority() const { return setup_priority::DATA; }

void BH1745Component::switch_led(bool on_off) {
  // shall we require somehow that its a pimoroni board?
  uint8_t raw = this->reg((uint8_t) Bh1745Registers::INTERRUPT_).get();

  if (on_off) {
    raw |= (1);
  } else {
    raw &= ~(1);
  }
  this->reg((uint8_t) Bh1745Registers::INTERRUPT_) = raw;
}

void BH1745Component::configure_measurement_time_() {
  ModeControl1Register mode_ctrl1;
  mode_ctrl1.reserved_3_7 = 0;
  mode_ctrl1.measurement_time = this->measurement_time_;
  this->reg((uint8_t) Bh1745Registers::MODE_CONTROL1) = mode_ctrl1.raw;
}

void BH1745Component::configure_gain_() {
  ModeControl2Register mode_ctrl2;
  mode_ctrl2.raw = this->reg((uint8_t) Bh1745Registers::MODE_CONTROL2).get();
  mode_ctrl2.adc_gain = this->adc_gain_;
  this->reg((uint8_t) Bh1745Registers::MODE_CONTROL2) = mode_ctrl2.raw;
}

bool BH1745Component::is_data_ready_(Readings &data) {
  ModeControl2Register mode_ctrl2;
  mode_ctrl2.raw = this->reg((uint8_t) Bh1745Registers::MODE_CONTROL2).get();
  if (mode_ctrl2.valid) {
    ModeControl1Register mode_ctrl1;
    mode_ctrl1.raw = this->reg((uint8_t) Bh1745Registers::MODE_CONTROL1).get();

    data.meas_time = mode_ctrl1.measurement_time;
    data.gain = mode_ctrl2.adc_gain;
  }
  return mode_ctrl2.valid;
}

void BH1745Component::read_data_(BH1745Component::Readings &data) {
  static uint8_t buffer[BH1745_CHANNELS * 2];

  this->read_bytes((uint8_t) Bh1745Registers::RED_DATA_LSB, buffer, BH1745_CHANNELS * 2);
  data.red = ((buffer[1] << 8) + buffer[0]) & 0xffff;
  data.green = ((buffer[3] << 8) + buffer[2]) & 0xffff;
  data.blue = ((buffer[5] << 8) + buffer[4]) & 0xffff;
  data.clear = ((buffer[7] << 8) + buffer[6]) & 0xffff;

  data.red *= CHANNEL_COMPENSATION[0];
  data.green *= CHANNEL_COMPENSATION[1];
  data.blue *= CHANNEL_COMPENSATION[2];
  data.clear *= CHANNEL_COMPENSATION[3];
  ESP_LOGD(TAG, "Red:%d,Green:%d,Blue:%d,Clear:%d", data.red, data.green, data.blue, data.clear);
}

float BH1745Component::calculate_lux_(Readings &data) {
  float lx, lx_tmp;
  float gain = get_adc_gain(data.gain);
  float integration_time = get_measurement_time_ms(data.meas_time);

  if (data.green < 1) {
    lx_tmp = 0;
  } else if (((float) data.clear / (float) data.green) < 0.160) {
    lx_tmp = (0.202 * data.red + 0.766 * data.green);
  } else {
    lx_tmp = (0.159 * data.red + 0.646 * data.green);
  }
  if (lx_tmp < 0) {
    lx_tmp = 0;
  }

  lx = lx_tmp / gain / integration_time * 160;
  ESP_LOGD(TAG, "Lux calculation:%.0f", lx);
  return lx;
}

float BH1745Component::calculate_cct_(Readings &data) {
  uint32_t all = data.red + data.green + data.blue;
  if (data.green < 1 || all < 1)
    return 0;
  float r_ratio = (float) data.red / all;
  float b_ratio = (float) data.blue / all;
  float ct = 0;
  if (((float) data.clear / (float) data.green) < 0.160) {
    float b_eff = fmin(b_ratio * 3.13, 1);
    ct = ((1 - b_eff) * 12746 * (exp(-2.911 * r_ratio))) + (b_eff * 1637 * (exp(4.865 * b_ratio)));

  } else {
    float b_eff = fmin(b_ratio * 10.67, 1);
    ct = ((1 - b_eff) * 16234 * (exp(-2.781 * r_ratio))) + (b_eff * 1882 * (exp(4.448 * b_ratio)));
  }
  if (ct > 10000)
    ct = 10000;
  ESP_LOGD(TAG, "CCT calculation:%.0f", ct);
  return roundf(ct);
  /*
  float cct;
  float gain = get_adc_gain(data.gain);
  float integration_time = get_measurement_time_ms(data.meas_time);
  float r, g, b, x, y, n;
  //copilot alg :)
    r = (float)data.red / data.clear;
    g = (float)data.green / data.clear;
    b = (float)data.blue / data.clear;

    x = (-0.14282) * r + (1.54924) * g + (-0.95641) * b;
    y = (-0.32466) * r + (1.57837) * g + (-0.73191) * b;
    n = (x - 0.3320) / (0.1858 - y);

    cct = (449 * n * n * n + 3525 * n * n + 6823.3 * n + 5520.33) / gain / integration_time;
    ESP_LOGD(TAG, "Red:%d,Green:%d,Blue:%d,Clear:%d,CCT calculation:%.0f\n", data.red, data.green, data.blue,
  data.clear, cct);
  */
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

}  // namespace bh1745
}  // namespace esphome
