#include "bh1750.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome::bh1750 {

static const char *const TAG = "bh1750.sensor";

static const uint8_t BH1750_COMMAND_POWER_ON = 0b00000001;
static const uint8_t BH1750_COMMAND_MT_REG_HI = 0b01000000;  // last 3 bits
static const uint8_t BH1750_COMMAND_MT_REG_LO = 0b01100000;  // last 5 bits
static const uint8_t BH1750_COMMAND_ONE_TIME_L = 0b00100011;
static const uint8_t BH1750_COMMAND_ONE_TIME_H = 0b00100000;
static const uint8_t BH1750_COMMAND_ONE_TIME_H2 = 0b00100001;

static constexpr uint32_t MEASUREMENT_TIMEOUT_MS = 2000;
static constexpr float HIGH_LIGHT_THRESHOLD_LX = 7000.0f;

// Measurement time constants (datasheet values)
static constexpr uint16_t MTREG_DEFAULT = 69;
static constexpr uint16_t MTREG_MIN = 31;
static constexpr uint16_t MTREG_MAX = 254;
static constexpr uint16_t MEAS_TIME_L_MS = 24;   // L-resolution max measurement time @ mtreg=69
static constexpr uint16_t MEAS_TIME_H_MS = 180;  // H/H2-resolution max measurement time @ mtreg=69

// Conversion constants (datasheet formulas)
static constexpr float RESOLUTION_DIVISOR = 1.2f;  // counts to lux conversion divisor
static constexpr float MODE_H2_DIVISOR = 2.0f;     // H2 mode has 2x higher resolution

// MTreg calculation constants
static constexpr int COUNTS_TARGET = 50000;  // Target counts for optimal range (avoid saturation)
static constexpr int COUNTS_NUMERATOR = 10;
static constexpr int COUNTS_DENOMINATOR = 12;

// MTreg register bit manipulation constants
static constexpr uint8_t MTREG_HI_SHIFT = 5;       // High 3 bits start at bit 5
static constexpr uint8_t MTREG_HI_MASK = 0b111;    // 3-bit mask for high bits
static constexpr uint8_t MTREG_LO_SHIFT = 0;       // Low 5 bits start at bit 0
static constexpr uint8_t MTREG_LO_MASK = 0b11111;  // 5-bit mask for low bits

/*
bh1750 properties:

L-resolution mode:
- resolution 4lx (@ mtreg=69)
- measurement time: typ=16ms, max=24ms, scaled by MTreg value divided by 69
- formula: counts / 1.2 * (69 / MTreg) lx
H-resolution mode:
- resolution 1lx (@ mtreg=69)
- measurement time: typ=120ms, max=180ms, scaled by MTreg value divided by 69
- formula: counts / 1.2 * (69 / MTreg) lx
H-resolution mode2:
- resolution 0.5lx (@ mtreg=69)
- measurement time: typ=120ms, max=180ms, scaled by MTreg value divided by 69
- formula: counts / 1.2 * (69 / MTreg) / 2 lx

MTreg:
- min=31, default=69, max=254

-> only reason to use l-resolution is faster, but offers no higher range
-> below ~7000lx, makes sense to use H-resolution2 @ MTreg=254
-> try to maximize MTreg to get lowest noise level
*/

void BH1750Sensor::setup() {
  uint8_t turn_on = BH1750_COMMAND_POWER_ON;
  if (this->write(&turn_on, 1) != i2c::ERROR_OK) {
    this->mark_failed();
    return;
  }
  this->state_ = IDLE;
}

void BH1750Sensor::dump_config() {
  LOG_SENSOR("", "BH1750", this);
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL_FOR, this->get_name().c_str());
  }

  LOG_UPDATE_INTERVAL(this);
}

void BH1750Sensor::update() {
  const uint32_t now = millis();

  // Start coarse measurement to determine optimal mode/mtreg
  if (this->state_ != IDLE) {
    // Safety timeout: reset if stuck
    if (now - this->measurement_start_time_ > MEASUREMENT_TIMEOUT_MS) {
      ESP_LOGW(TAG, "Measurement timeout, resetting state");
      this->state_ = IDLE;
    } else {
      ESP_LOGW(TAG, "Previous measurement not complete, skipping update");
      return;
    }
  }

  if (!this->start_measurement_(BH1750_MODE_L, MTREG_MIN, now)) {
    this->status_set_warning();
    this->publish_state(NAN);
    return;
  }

  this->state_ = WAITING_COARSE_MEASUREMENT;
  this->enable_loop();  // Enable loop while measurement in progress
}

void BH1750Sensor::loop() {
  const uint32_t now = App.get_loop_component_start_time();

  switch (this->state_) {
    case IDLE:
      // Disable loop when idle to save cycles
      this->disable_loop();
      break;

    case WAITING_COARSE_MEASUREMENT:
      if (now - this->measurement_start_time_ >= this->measurement_duration_) {
        this->state_ = READING_COARSE_RESULT;
      }
      break;

    case READING_COARSE_RESULT: {
      float lx;
      if (!this->read_measurement_(lx)) {
        this->fail_and_reset_();
        break;
      }

      this->process_coarse_result_(lx);

      // Start fine measurement with optimal settings
      // fetch millis() again since the read can take a bit
      if (!this->start_measurement_(this->fine_mode_, this->fine_mtreg_, millis())) {
        this->fail_and_reset_();
        break;
      }

      this->state_ = WAITING_FINE_MEASUREMENT;
      break;
    }

    case WAITING_FINE_MEASUREMENT:
      if (now - this->measurement_start_time_ >= this->measurement_duration_) {
        this->state_ = READING_FINE_RESULT;
      }
      break;

    case READING_FINE_RESULT: {
      float lx;
      if (!this->read_measurement_(lx)) {
        this->fail_and_reset_();
        break;
      }

      ESP_LOGD(TAG, "'%s': Illuminance=%.1flx", this->get_name().c_str(), lx);
      this->status_clear_warning();
      this->publish_state(lx);
      this->state_ = IDLE;
      break;
    }
  }
}

bool BH1750Sensor::start_measurement_(BH1750Mode mode, uint8_t mtreg, uint32_t now) {
  // Power on
  uint8_t turn_on = BH1750_COMMAND_POWER_ON;
  if (this->write(&turn_on, 1) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Power on failed");
    return false;
  }

  // Set MTreg if changed
  if (this->active_mtreg_ != mtreg) {
    uint8_t mtreg_hi = BH1750_COMMAND_MT_REG_HI | ((mtreg >> MTREG_HI_SHIFT) & MTREG_HI_MASK);
    uint8_t mtreg_lo = BH1750_COMMAND_MT_REG_LO | ((mtreg >> MTREG_LO_SHIFT) & MTREG_LO_MASK);
    if (this->write(&mtreg_hi, 1) != i2c::ERROR_OK || this->write(&mtreg_lo, 1) != i2c::ERROR_OK) {
      ESP_LOGW(TAG, "Set measurement time failed");
      this->active_mtreg_ = 0;
      return false;
    }
    this->active_mtreg_ = mtreg;
  }

  // Start measurement
  uint8_t cmd;
  uint16_t meas_time;
  switch (mode) {
    case BH1750_MODE_L:
      cmd = BH1750_COMMAND_ONE_TIME_L;
      meas_time = MEAS_TIME_L_MS * mtreg / MTREG_DEFAULT;
      break;
    case BH1750_MODE_H:
      cmd = BH1750_COMMAND_ONE_TIME_H;
      meas_time = MEAS_TIME_H_MS * mtreg / MTREG_DEFAULT;
      break;
    case BH1750_MODE_H2:
      cmd = BH1750_COMMAND_ONE_TIME_H2;
      meas_time = MEAS_TIME_H_MS * mtreg / MTREG_DEFAULT;
      break;
    default:
      return false;
  }

  if (this->write(&cmd, 1) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Start measurement failed");
    return false;
  }

  // Store current measurement parameters
  this->current_mode_ = mode;
  this->current_mtreg_ = mtreg;
  this->measurement_start_time_ = now;
  this->measurement_duration_ = meas_time + 1;  // Add 1ms for safety

  return true;
}

bool BH1750Sensor::read_measurement_(float &lx_out) {
  uint16_t raw_value;
  if (this->read(reinterpret_cast<uint8_t *>(&raw_value), 2) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Read data failed");
    return false;
  }
  raw_value = i2c::i2ctohs(raw_value);

  float lx = float(raw_value) / RESOLUTION_DIVISOR;
  lx *= float(MTREG_DEFAULT) / this->current_mtreg_;
  if (this->current_mode_ == BH1750_MODE_H2) {
    lx /= MODE_H2_DIVISOR;
  }

  lx_out = lx;
  return true;
}

void BH1750Sensor::process_coarse_result_(float lx) {
  if (std::isnan(lx)) {
    // Use defaults if coarse measurement failed
    this->fine_mode_ = BH1750_MODE_H2;
    this->fine_mtreg_ = MTREG_MAX;
    return;
  }

  if (lx <= HIGH_LIGHT_THRESHOLD_LX) {
    this->fine_mode_ = BH1750_MODE_H2;
    this->fine_mtreg_ = MTREG_MAX;
  } else {
    this->fine_mode_ = BH1750_MODE_H;
    // lx = counts / 1.2 * (69 / mtreg)
    // -> mtreg = counts / 1.2 * (69 / lx)
    // calculate for counts=50000 (allow some range to not saturate, but maximize mtreg)
    // -> mtreg = 50000*(10/12)*(69/lx)
    int ideal_mtreg = COUNTS_TARGET * COUNTS_NUMERATOR * MTREG_DEFAULT / (COUNTS_DENOMINATOR * (int) lx);
    this->fine_mtreg_ = std::min((int) MTREG_MAX, std::max((int) MTREG_MIN, ideal_mtreg));
  }

  ESP_LOGV(TAG, "L result: %.1f -> Calculated mode=%d, mtreg=%d", lx, (int) this->fine_mode_, this->fine_mtreg_);
}

void BH1750Sensor::fail_and_reset_() {
  this->status_set_warning();
  this->publish_state(NAN);
  this->state_ = IDLE;
}

float BH1750Sensor::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace esphome::bh1750
