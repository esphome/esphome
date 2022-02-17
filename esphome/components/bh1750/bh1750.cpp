#include "bh1750.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bh1750 {

static const char *const TAG = "bh1750.sensor";

static const uint8_t BH1750_COMMAND_POWER_ON = 0b00000001;
static const uint8_t BH1750_COMMAND_MT_REG_HI = 0b01000000;  // last 3 bits
static const uint8_t BH1750_COMMAND_MT_REG_LO = 0b01100000;  // last 5 bits
static const uint8_t BH1750_COMMAND_ONE_TIME_L = 0b00100011;
static const uint8_t BH1750_COMMAND_ONE_TIME_H = 0b00100000;
static const uint8_t BH1750_COMMAND_ONE_TIME_H2 = 0b00100001;

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
  ESP_LOGCONFIG(TAG, "Setting up BH1750 '%s'...", this->name_.c_str());
  uint8_t turn_on = BH1750_COMMAND_POWER_ON;
  if (this->write(&turn_on, 1) != i2c::ERROR_OK) {
    this->mark_failed();
    return;
  }
}

void BH1750Sensor::read_lx_(BH1750Mode mode, uint8_t mtreg, const std::function<void(float)> &f) {
  // turn on (after one-shot sensor automatically powers down)
  uint8_t turn_on = BH1750_COMMAND_POWER_ON;
  if (this->write(&turn_on, 1) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Turning on BH1750 failed");
    f(NAN);
    return;
  }

  if (active_mtreg_ != mtreg) {
    // set mtreg
    uint8_t mtreg_hi = BH1750_COMMAND_MT_REG_HI | ((mtreg >> 5) & 0b111);
    uint8_t mtreg_lo = BH1750_COMMAND_MT_REG_LO | ((mtreg >> 0) & 0b11111);
    if (this->write(&mtreg_hi, 1) != i2c::ERROR_OK || this->write(&mtreg_lo, 1) != i2c::ERROR_OK) {
      ESP_LOGW(TAG, "Setting measurement time for BH1750 failed");
      active_mtreg_ = 0;
      f(NAN);
      return;
    }
    active_mtreg_ = mtreg;
  }

  uint8_t cmd;
  uint16_t meas_time;
  switch (mode) {
    case BH1750_MODE_L:
      cmd = BH1750_COMMAND_ONE_TIME_L;
      meas_time = 24 * mtreg / 69;
      break;
    case BH1750_MODE_H:
      cmd = BH1750_COMMAND_ONE_TIME_H;
      meas_time = 180 * mtreg / 69;
      break;
    case BH1750_MODE_H2:
      cmd = BH1750_COMMAND_ONE_TIME_H2;
      meas_time = 180 * mtreg / 69;
      break;
    default:
      f(NAN);
      return;
  }
  if (this->write(&cmd, 1) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Starting measurement for BH1750 failed");
    f(NAN);
    return;
  }

  // probably not needed, but adjust for rounding
  meas_time++;

  this->set_timeout("read", meas_time, [this, mode, mtreg, f]() {
    uint16_t raw_value;
    if (this->read(reinterpret_cast<uint8_t *>(&raw_value), 2) != i2c::ERROR_OK) {
      ESP_LOGW(TAG, "Reading BH1750 data failed");
      f(NAN);
      return;
    }
    raw_value = i2c::i2ctohs(raw_value);

    float lx = float(raw_value) / 1.2f;
    lx *= 69.0f / mtreg;
    if (mode == BH1750_MODE_H2)
      lx /= 2.0f;

    f(lx);
  });
}

void BH1750Sensor::dump_config() {
  LOG_SENSOR("", "BH1750", this);
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with BH1750 failed!");
  }

  LOG_UPDATE_INTERVAL(this);
}

void BH1750Sensor::update() {
  // first do a quick measurement in L-mode with full range
  // to find right range
  this->read_lx_(BH1750_MODE_L, 31, [this](float val) {
    if (std::isnan(val)) {
      this->status_set_warning();
      this->publish_state(NAN);
      return;
    }

    BH1750Mode use_mode;
    uint8_t use_mtreg;
    if (val <= 7000) {
      use_mode = BH1750_MODE_H2;
      use_mtreg = 254;
    } else {
      use_mode = BH1750_MODE_H;
      // lx = counts / 1.2 * (69 / mtreg)
      // -> mtreg = counts / 1.2 * (69 / lx)
      // calculate for counts=50000 (allow some range to not saturate, but maximize mtreg)
      // -> mtreg = 50000*(10/12)*(69/lx)
      int ideal_mtreg = 50000 * 10 * 69 / (12 * (int) val);
      use_mtreg = std::min(254, std::max(31, ideal_mtreg));
    }
    ESP_LOGV(TAG, "L result: %f -> Calculated mode=%d, mtreg=%d", val, (int) use_mode, use_mtreg);

    this->read_lx_(use_mode, use_mtreg, [this](float val) {
      if (std::isnan(val)) {
        this->status_set_warning();
        this->publish_state(NAN);
        return;
      }
      ESP_LOGD(TAG, "'%s': Got illuminance=%.1flx", this->get_name().c_str(), val);
      this->status_clear_warning();
      this->publish_state(val);
    });
  });
}

float BH1750Sensor::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace bh1750
}  // namespace esphome
