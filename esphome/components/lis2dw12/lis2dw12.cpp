#include "lis2dw12.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::lis2dw12 {

ESPHOME_LOG_TAG(TAG, "lis2dw12");

static constexpr uint8_t LIS2DW12_WHO_AM_I_VALUE = 0x44;
static constexpr float GRAVITY_EARTH = 9.80665f;
static constexpr uint32_t EVENT_COOLDOWN_MS = 500;
static constexpr uint32_t POLL_INTERVAL_MS = 50;
static constexpr uint8_t SIXD_POSITION_MASK = 0x3F;  // XL..ZH, leaving out D6D_IA

static const LogString *power_mode_to_string(PowerMode mode) {
  switch (mode) {
    case PowerMode::HIGH_PERF:
      return LOG_STR("High Performance");
    case PowerMode::LOW_POWER_1:
      return LOG_STR("Low Power 1 (12-bit)");
    case PowerMode::LOW_POWER_2:
      return LOG_STR("Low Power 2");
    case PowerMode::LOW_POWER_3:
      return LOG_STR("Low Power 3");
    case PowerMode::LOW_POWER_4:
      return LOG_STR("Low Power 4");
  }
  return LOG_STR("Unknown");
}

static const LogString *range_to_string(Range range) {
  switch (range) {
    case Range::RANGE_2G:
      return LOG_STR("±2g");
    case Range::RANGE_4G:
      return LOG_STR("±4g");
    case Range::RANGE_8G:
      return LOG_STR("±8g");
    case Range::RANGE_16G:
      return LOG_STR("±16g");
  }
  return LOG_STR("Unknown");
}

static const LogString *data_rate_to_string(DataRate rate) {
  switch (rate) {
    case DataRate::RATE_1_6HZ:
      return LOG_STR("1.6 Hz");
    case DataRate::RATE_12_5HZ:
      return LOG_STR("12.5 Hz");
    case DataRate::RATE_25HZ:
      return LOG_STR("25 Hz");
    case DataRate::RATE_50HZ:
      return LOG_STR("50 Hz");
    case DataRate::RATE_100HZ:
      return LOG_STR("100 Hz");
    case DataRate::RATE_200HZ:
      return LOG_STR("200 Hz");
    case DataRate::RATE_400HZ:
      return LOG_STR("400 Hz");
    case DataRate::RATE_800HZ:
      return LOG_STR("800 Hz");
    case DataRate::RATE_1600HZ:
      return LOG_STR("1600 Hz");
  }
  return LOG_STR("Unknown");
}

static const LogString *filter_bandwidth_to_string(FilterBandwidth bw) {
  switch (bw) {
    case FilterBandwidth::BW_ODR_DIV_2:
      return LOG_STR("ODR/2");
    case FilterBandwidth::BW_ODR_DIV_4:
      return LOG_STR("ODR/4");
    case FilterBandwidth::BW_ODR_DIV_10:
      return LOG_STR("ODR/10");
    case FilterBandwidth::BW_ODR_DIV_20:
      return LOG_STR("ODR/20");
  }
  return LOG_STR("Unknown");
}

void LIS2DW12Component::setup() {
  uint8_t who_am_i{0};
  if (!this->read_byte(static_cast<uint8_t>(RegisterMap::WHO_AM_I), &who_am_i)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  if (who_am_i != LIS2DW12_WHO_AM_I_VALUE) {
    ESP_LOGE(TAG, "WHO_AM_I check failed. Got 0x%02X, expected 0x%02X", who_am_i, LIS2DW12_WHO_AM_I_VALUE);
    this->mark_failed(LOG_STR("Wrong chip ID"));
    return;
  }

  // Soft reset
  if (!this->write_reg_(RegisterMap::CTRL2, 0x40)) {
    this->mark_failed(LOG_STR("Soft reset failed"));
    return;
  }
  delay(1);  // NOLINT - wait for reset to complete

  uint8_t ctrl2{0};
  if (!this->read_byte(static_cast<uint8_t>(RegisterMap::CTRL2), &ctrl2) || (ctrl2 & 0x40)) {
    this->mark_failed(LOG_STR("Soft reset did not complete"));
    return;
  }

  if (!this->configure_registers_()) {
    this->mark_failed(LOG_STR("Register configuration failed"));
    return;
  }

  // 0.244 mg/LSB at ±2g (14-bit), doubling per range step; 12-bit LP1 is 4x coarser
  const bool lp1 = this->power_mode_ == PowerMode::LOW_POWER_1;
  const uint8_t range_idx = static_cast<uint8_t>(this->range_);
  const float mg = 0.244f * (1u << range_idx) * (lp1 ? 4 : 1);
  this->scale_ = mg * 0.001f * GRAVITY_EARTH;
  this->shift_ = lp1 ? 4 : 2;

  bool events_used = !this->tap_callback_.empty() || !this->double_tap_callback_.empty() ||
                     !this->freefall_callback_.empty() || !this->active_callback_.empty() ||
                     !this->orientation_callback_.empty();
#ifdef USE_BINARY_SENSOR
  for (auto *sensor : this->event_binary_sensors_) {
    if (sensor != nullptr) {
      sensor->publish_initial_state(false);
      events_used = true;
    }
  }
#endif
#ifdef USE_TEXT_SENSOR
  RegSixdSrc sixd;
  if (this->orientation_text_sensor_ != nullptr &&
      this->read_byte(static_cast<uint8_t>(RegisterMap::SIXD_SRC), &sixd.raw)) {
    this->publish_orientation_(sixd);
    events_used = true;
  }
#endif
  if (!events_used) {
    this->disable_loop();  // nothing consumes the events, so skip polling the chip
  }
}

bool LIS2DW12Component::configure_registers_() {
  // CTRL1: ODR[7:4], MODE[3:2] (00=low power, 01=high performance), LP_MODE[1:0] (LP1..LP4)
  const uint8_t odr_bits = static_cast<uint8_t>(this->data_rate_) << 4;
  uint8_t ctrl1;
  if (this->power_mode_ == PowerMode::HIGH_PERF) {
    ctrl1 = odr_bits | 0x04;
  } else {
    ctrl1 = odr_bits | ((static_cast<uint8_t>(this->power_mode_) - 1) & 0x03);
  }

  // CTRL6: BW_FILT[7:6], FS[5:4], LOW_NOISE[2]
  uint8_t ctrl6 = (static_cast<uint8_t>(this->filter_bandwidth_) << 6) | (static_cast<uint8_t>(this->range_) << 4);
  if (this->low_noise_) {
    ctrl6 |= 0x04;
  }

  return this->write_reg_(RegisterMap::CTRL2, 0x0C) &&        // BDU, IF_ADD_INC
         this->write_reg_(RegisterMap::CTRL1, ctrl1) &&       // ODR and power mode
         this->write_reg_(RegisterMap::CTRL6, ctrl6) &&       // bandwidth, range, low noise
         this->write_reg_(RegisterMap::TAP_THS_X, 0x29) &&    // 6D_THS=70 degrees, TAP_THSX=9
         this->write_reg_(RegisterMap::TAP_THS_Y, 0x09) &&    // TAP_THSY=9
         this->write_reg_(RegisterMap::TAP_THS_Z, 0xE9) &&    // TAP_THSZ=9, tap on X/Y/Z
         this->write_reg_(RegisterMap::INT_DUR, 0x76) &&      // LATENCY=7, QUIET=1, SHOCK=2
         this->write_reg_(RegisterMap::WAKE_UP_THS, 0x82) &&  // double tap enabled, WU_THS=2
         this->write_reg_(RegisterMap::WAKE_UP_DUR, 0x00) &&  // no wake/sleep durations
         this->write_reg_(RegisterMap::FREE_FALL, 0x33) &&    // FF_DUR=6, FF_THS=312 mg
         this->write_reg_(RegisterMap::CTRL4_INT1, 0xF8) &&   // 6D, single tap, wake-up, free-fall, double tap to INT1
         this->write_reg_(RegisterMap::CTRL3, 0x10) &&        // LIR: latched interrupts
         this->write_reg_(RegisterMap::CTRL7, 0x20);          // INTERRUPTS_ENABLE
}

void LIS2DW12Component::dump_config() {
  ESP_LOGCONFIG(TAG, "LIS2DW12:");
  ESP_LOGCONFIG(TAG,
                "  Power Mode: %s\n"
                "  Data Rate: %s\n"
                "  Range: %s\n"
                "  Filter Bandwidth: %s\n"
                "  Low Noise: %s\n"
                "  Offsets: {%.3f m/s², %.3f m/s², %.3f m/s²}\n"
                "  Transform: {mirror_x=%s, mirror_y=%s, mirror_z=%s, swap_xy=%s}",
                LOG_STR_ARG(power_mode_to_string(this->power_mode_)),
                LOG_STR_ARG(data_rate_to_string(this->data_rate_)), LOG_STR_ARG(range_to_string(this->range_)),
                LOG_STR_ARG(filter_bandwidth_to_string(this->filter_bandwidth_)), YESNO(this->low_noise_),
                this->offset_x_, this->offset_y_, this->offset_z_, YESNO(this->mirror_x_), YESNO(this->mirror_y_),
                YESNO(this->mirror_z_), YESNO(this->swap_xy_));
  LOG_UPDATE_INTERVAL(this);

#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Tap", this->event_binary_sensors_[EVENT_TAP]);
  LOG_BINARY_SENSOR("  ", "Double Tap", this->event_binary_sensors_[EVENT_DOUBLE_TAP]);
  LOG_BINARY_SENSOR("  ", "Freefall", this->event_binary_sensors_[EVENT_FREEFALL]);
  LOG_BINARY_SENSOR("  ", "Active", this->event_binary_sensors_[EVENT_ACTIVE]);
#endif

#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Acceleration X", this->acceleration_x_sensor_);
  LOG_SENSOR("  ", "Acceleration Y", this->acceleration_y_sensor_);
  LOG_SENSOR("  ", "Acceleration Z", this->acceleration_z_sensor_);
#endif

#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "Orientation", this->orientation_text_sensor_);
#endif
}

bool LIS2DW12Component::read_acceleration_data_(float &x, float &y, float &z) {
  uint8_t accel_data[6];
  if (!this->read_bytes(static_cast<uint8_t>(RegisterMap::OUT_X_L), accel_data, 6)) {
    return false;
  }

  int16_t raw_x = encode_value_le<int16_t>(accel_data) >> this->shift_;
  int16_t raw_y = encode_value_le<int16_t>(&accel_data[2]) >> this->shift_;
  int16_t raw_z = encode_value_le<int16_t>(&accel_data[4]) >> this->shift_;

  if (this->swap_xy_) {
    std::swap(raw_x, raw_y);
  }
  x = (this->mirror_x_ ? -raw_x : raw_x) * this->scale_ + this->offset_x_;
  y = (this->mirror_y_ ? -raw_y : raw_y) * this->scale_ + this->offset_y_;
  z = (this->mirror_z_ ? -raw_z : raw_z) * this->scale_ + this->offset_z_;
  return true;
}

#ifdef USE_TEXT_SENSOR
static const char *orientation_to_string(RegSixdSrc sixd) {
  if (sixd.xh)
    return "X Up";
  if (sixd.xl)
    return "X Down";
  if (sixd.yh)
    return "Y Up";
  if (sixd.yl)
    return "Y Down";
  if (sixd.zh)
    return "Z Up";
  if (sixd.zl)
    return "Z Down";
  return "Unknown";
}

void LIS2DW12Component::publish_orientation_(RegSixdSrc sixd) {
  const uint8_t position = sixd.raw & SIXD_POSITION_MASK;
  if (this->orientation_text_sensor_ == nullptr || position == this->sixd_last_) {
    return;
  }
  this->sixd_last_ = position;
  this->orientation_text_sensor_->publish_state(orientation_to_string(sixd));
}
#endif

void LIS2DW12Component::handle_event_(Event event, bool active, uint32_t now, LazyCallbackManager<void()> &callback) {
  const uint8_t bit = 1 << event;
  bool state;
  if (active) {
    if (now - this->last_event_ms_[event] <= EVENT_COOLDOWN_MS) {
      return;
    }
    this->last_event_ms_[event] = now;
    callback.call();
    if (this->events_on_ & bit) {
      return;
    }
    this->events_on_ |= bit;
    state = true;
  } else {
    if (!(this->events_on_ & bit) || now - this->last_event_ms_[event] <= EVENT_COOLDOWN_MS) {
      return;
    }
    this->events_on_ &= ~bit;
    state = false;
  }
#ifdef USE_BINARY_SENSOR
  if (this->event_binary_sensors_[event] != nullptr) {
    this->event_binary_sensors_[event]->publish_state(state);
  }
#endif
}

void LIS2DW12Component::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  if (now - this->last_poll_ms_ < POLL_INTERVAL_MS) {
    return;
  }
  this->last_poll_ms_ = now;

  // SIXD_SRC and ALL_INT_SRC are adjacent, so one read captures both before any latch clears
  uint8_t regs[2];
  if (!this->status_comm_result(this->read_bytes(static_cast<uint8_t>(RegisterMap::SIXD_SRC), regs, sizeof(regs))))
    return;
  RegSixdSrc sixd;
  sixd.raw = regs[0];
  RegAllIntSrc src;
  src.raw = regs[1];

  this->handle_event_(EVENT_TAP, src.single_tap, now, this->tap_callback_);
  this->handle_event_(EVENT_DOUBLE_TAP, src.double_tap, now, this->double_tap_callback_);
  this->handle_event_(EVENT_FREEFALL, src.ff_ia, now, this->freefall_callback_);
  this->handle_event_(EVENT_ACTIVE, src.wu_ia, now, this->active_callback_);

  if (sixd.d6d_ia) {
    this->orientation_callback_.call();
#ifdef USE_TEXT_SENSOR
    this->publish_orientation_(sixd);
#endif
  }
}

void LIS2DW12Component::update() {
  float x{}, y{}, z{};
  if (!this->status_comm_result(this->read_acceleration_data_(x, y, z)))
    return;

  ESP_LOGV(TAG, "Acceleration: {x = %+1.3f m/s², y = %+1.3f m/s², z = %+1.3f m/s²}", x, y, z);

#ifdef USE_SENSOR
  PUBLISH_STATE(this->acceleration_x_sensor_, x);
  PUBLISH_STATE(this->acceleration_y_sensor_, y);
  PUBLISH_STATE(this->acceleration_z_sensor_, z);
#endif
}

void LIS2DW12Component::set_offset(float offset_x, float offset_y, float offset_z) {
  this->offset_x_ = offset_x;
  this->offset_y_ = offset_y;
  this->offset_z_ = offset_z;
}

void LIS2DW12Component::set_transform(bool mirror_x, bool mirror_y, bool mirror_z, bool swap_xy) {
  this->mirror_x_ = mirror_x;
  this->mirror_y_ = mirror_y;
  this->mirror_z_ = mirror_z;
  this->swap_xy_ = swap_xy;
}

}  // namespace esphome::lis2dw12
