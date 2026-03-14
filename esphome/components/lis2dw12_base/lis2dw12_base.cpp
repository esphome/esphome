#include "lis2dw12_base.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::lis2dw12_base {

static const char *const TAG = "lis2dw12_base";

static const uint8_t LIS2DW12_WHO_AM_I_VALUE = 0x44;
static const float GRAVITY_EARTH = 9.80665f;

static const uint32_t TAP_COOLDOWN_MS = 500;
static const uint32_t DOUBLE_TAP_COOLDOWN_MS = 500;
static const uint32_t FREEFALL_COOLDOWN_MS = 500;
static const uint32_t ACTIVITY_COOLDOWN_MS = 500;

// Sensitivity in mg/LSB for 14-bit (high-perf, LP2-4)
// Index by Range enum value: 2G=0, 4G=1, 8G=2, 16G=3
static const float SENSITIVITY_14BIT[] = {0.244f, 0.488f, 0.976f, 1.952f};
// Sensitivity for 12-bit (LP1 mode)
static const float SENSITIVITY_12BIT[] = {0.976f, 1.952f, 3.904f, 7.808f};

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
    default:
      return LOG_STR("Unknown");
  }
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
    default:
      return LOG_STR("Unknown");
  }
}

static const LogString *data_rate_to_string(DataRate rate) {
  switch (rate) {
    case DataRate::RATE_POWER_DOWN:
      return LOG_STR("Power Down");
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
    default:
      return LOG_STR("Unknown");
  }
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
    default:
      return LOG_STR("Unknown");
  }
}

void LIS2DW12Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up LIS2DW12...");

  // Verify WHO_AM_I
  uint8_t who_am_i{0};
  if (!this->read_byte(static_cast<uint8_t>(RegisterMap::WHO_AM_I), &who_am_i) || who_am_i != LIS2DW12_WHO_AM_I_VALUE) {
    ESP_LOGE(TAG, "WHO_AM_I check failed. Got 0x%02X, expected 0x%02X", who_am_i, LIS2DW12_WHO_AM_I_VALUE);
    this->mark_failed();
    return;
  }

  // Soft reset
  if (!this->write_byte(static_cast<uint8_t>(RegisterMap::CTRL2), 0x40)) {
    ESP_LOGE(TAG, "Soft reset failed");
    this->mark_failed();
    return;
  }
  delay(1);  // NOLINT - wait for reset to complete

  // Verify reset complete
  uint8_t ctrl2{0};
  if (!this->read_byte(static_cast<uint8_t>(RegisterMap::CTRL2), &ctrl2) || (ctrl2 & 0x40)) {
    ESP_LOGE(TAG, "Soft reset did not complete");
    this->mark_failed();
    return;
  }

  if (!this->configure_registers_()) {
    ESP_LOGE(TAG, "Register configuration failed");
    this->mark_failed();
    return;
  }

  // Set sensitivity based on range and power mode
  uint8_t range_idx = static_cast<uint8_t>(this->range_);
  if (this->power_mode_ == PowerMode::LOW_POWER_1) {
    this->sensitivity_ = SENSITIVITY_12BIT[range_idx];
  } else {
    this->sensitivity_ = SENSITIVITY_14BIT[range_idx];
  }
}

bool LIS2DW12Component::configure_registers_() {
  // CTRL2: BDU=1, IF_ADD_INC=1
  // Bit 3: BDU, Bit 2: IF_ADD_INC
  if (!this->write_byte(static_cast<uint8_t>(RegisterMap::CTRL2), 0x0C)) {
    return false;
  }

  // CTRL1: ODR[7:4] + MODE[3:2] + LP_MODE[1:0]
  // For High Performance: MODE=0b01 (high performance / low power), LP_MODE doesn't matter for HP
  // CTRL1 bits: ODR[7:4], MODE[3:2], LP_MODE[1:0]
  // MODE: 00=Low-power, 01=High-performance, 10=Single data conversion
  // LP_MODE: 00=LP1 (12-bit), 01=LP2 (14-bit), 10=LP3 (14-bit), 11=LP4 (14-bit)
  uint8_t ctrl1 = 0;
  uint8_t odr_bits = static_cast<uint8_t>(this->data_rate_) << 4;
  if (this->power_mode_ == PowerMode::HIGH_PERF) {
    ctrl1 = odr_bits | 0x04;  // MODE=01, LP_MODE=00
  } else {
    // Low power modes: MODE=00, LP_MODE = power_mode - 1 (LP1=0, LP2=1, LP3=2, LP4=3)
    uint8_t lp_mode = static_cast<uint8_t>(this->power_mode_) - 1;
    ctrl1 = odr_bits | (lp_mode & 0x03);
  }
  if (!this->write_byte(static_cast<uint8_t>(RegisterMap::CTRL1), ctrl1)) {
    return false;
  }

  // CTRL6: BW_FILT[7:6] + FS[5:4] + FDS[3] + LOW_NOISE[2]
  uint8_t ctrl6 = 0;
  ctrl6 |= (static_cast<uint8_t>(this->filter_bandwidth_) << 6);
  ctrl6 |= (static_cast<uint8_t>(this->range_) << 4);
  if (this->low_noise_) {
    ctrl6 |= 0x04;
  }
  if (!this->write_byte(static_cast<uint8_t>(RegisterMap::CTRL6), ctrl6)) {
    return false;
  }

  // Configure tap detection
  // TAP_THS_X: 6D_THS[6:5]=01 (60 degrees), TAP_THSX[4:0]=0x09
  if (!this->write_byte(static_cast<uint8_t>(RegisterMap::TAP_THS_X), 0x29)) {
    return false;
  }
  // TAP_THS_Y: TAP_THSY[4:0]=0x09
  if (!this->write_byte(static_cast<uint8_t>(RegisterMap::TAP_THS_Y), 0x09)) {
    return false;
  }
  // TAP_THS_Z: TAP_THSZ[4:0]=0x09, TAP_X_EN, TAP_Y_EN, TAP_Z_EN
  if (!this->write_byte(static_cast<uint8_t>(RegisterMap::TAP_THS_Z), 0xE9)) {
    return false;
  }

  // INT_DUR: LATENCY[7:4]=0x7, QUIET[3:2]=0x1, SHOCK[1:0]=0x2
  if (!this->write_byte(static_cast<uint8_t>(RegisterMap::INT_DUR), 0x76)) {
    return false;
  }

  // WAKE_UP_THS: SINGLE_DOUBLE_TAP[7]=1 (enable double tap), WU_THS[5:0]=0x02
  if (!this->write_byte(static_cast<uint8_t>(RegisterMap::WAKE_UP_THS), 0x82)) {
    return false;
  }

  // WAKE_UP_DUR: FF_DUR5[7]=0, WAKE_DUR[6:5]=0, SLEEP_DUR[3:0]=0
  if (!this->write_byte(static_cast<uint8_t>(RegisterMap::WAKE_UP_DUR), 0x00)) {
    return false;
  }

  // FREE_FALL: FF_DUR[7:3]=0x06, FF_THS[2:0]=0x03 (312mg threshold)
  if (!this->write_byte(static_cast<uint8_t>(RegisterMap::FREE_FALL), 0x33)) {
    return false;
  }

  // CTRL4_INT1: route interrupts to INT1
  // INT1_6D[7]=1, INT1_SINGLE_TAP[6]=1, INT1_WU[5]=1, INT1_FF[4]=1,
  // INT1_TAP[3]=1, INT1_DIFF5[2]=0, INT1_FTH[1]=0, INT1_DRDY[0]=0
  if (!this->write_byte(static_cast<uint8_t>(RegisterMap::CTRL4_INT1), 0xF8)) {
    return false;
  }

  // CTRL3: SLP_MODE_SEL[1]=0, LIR[4]=1 (latched interrupts)
  if (!this->write_byte(static_cast<uint8_t>(RegisterMap::CTRL3), 0x10)) {
    return false;
  }

  // CTRL7: INTERRUPTS_ENABLE[5]=1, USR_OFF_ON_OUT[4]=0
  // Also enable tap/6D/FF/WU interrupts
  if (!this->write_byte(static_cast<uint8_t>(RegisterMap::CTRL7), 0x20)) {
    return false;
  }

  return true;
}

void LIS2DW12Component::dump_config() {
  ESP_LOGCONFIG(TAG, "LIS2DW12:");
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
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
  LOG_BINARY_SENSOR("  ", "Tap", this->tap_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Double Tap", this->double_tap_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Freefall", this->freefall_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Active", this->active_binary_sensor_);
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

bool LIS2DW12Component::read_acceleration_data_() {
  uint8_t accel_data[6];
  if (!this->read_bytes(static_cast<uint8_t>(RegisterMap::OUT_X_L), accel_data, 6)) {
    return false;
  }

  // LIS2DW12 data is left-justified in 16-bit words
  // For 14-bit: data in bits [15:2], so raw int16 >> 2 gives 14-bit signed value
  // For 12-bit (LP1): data in bits [15:4], so raw int16 >> 4 gives 12-bit signed value
  int16_t raw_x = static_cast<int16_t>((accel_data[1] << 8) | accel_data[0]);
  int16_t raw_y = static_cast<int16_t>((accel_data[3] << 8) | accel_data[2]);
  int16_t raw_z = static_cast<int16_t>((accel_data[5] << 8) | accel_data[4]);

  if (this->power_mode_ == PowerMode::LOW_POWER_1) {
    raw_x >>= 4;
    raw_y >>= 4;
    raw_z >>= 4;
  } else {
    raw_x >>= 2;
    raw_y >>= 2;
    raw_z >>= 2;
  }

  // Apply axis transform
  if (this->swap_xy_) {
    std::swap(raw_x, raw_y);
  }
  if (this->mirror_x_) {
    raw_x = -raw_x;
  }
  if (this->mirror_y_) {
    raw_y = -raw_y;
  }
  if (this->mirror_z_) {
    raw_z = -raw_z;
  }

  // Convert to m/s^2: raw * sensitivity_mg_per_lsb * 0.001 * GRAVITY_EARTH
  float factor = this->sensitivity_ * 0.001f * GRAVITY_EARTH;
  this->data_.x = raw_x * factor + this->offset_x_;
  this->data_.y = raw_y * factor + this->offset_y_;
  this->data_.z = raw_z * factor + this->offset_z_;

  return true;
}

bool LIS2DW12Component::read_interrupt_status_() {
  uint8_t all_int_src = 0;
  uint8_t sixd_src = 0;
  bool ok = this->read_byte(static_cast<uint8_t>(RegisterMap::ALL_INT_SRC), &all_int_src);
  ok = this->read_byte(static_cast<uint8_t>(RegisterMap::SIXD_SRC), &sixd_src) && ok;
  if (!ok) {
    this->status_.all_int.raw = 0;
    this->status_.sixd.raw = 0;
    return false;
  }
  this->status_.all_int.raw = all_int_src;
  this->status_.sixd.raw = sixd_src;
  return true;
}

const char *LIS2DW12Component::get_orientation_string_() {
  if (this->status_.sixd.xh)
    return "X Up";
  if (this->status_.sixd.xl)
    return "X Down";
  if (this->status_.sixd.yh)
    return "Y Up";
  if (this->status_.sixd.yl)
    return "Y Down";
  if (this->status_.sixd.zh)
    return "Z Up";
  if (this->status_.sixd.zl)
    return "Z Down";
  return "Unknown";
}

static void binary_event_debounce(bool state, uint32_t now, uint32_t &last_ms, Trigger<> &trigger, uint32_t cooldown_ms,
                                  void *bs, const char *desc) {
  if (state && now - last_ms > cooldown_ms) {
    ESP_LOGV(TAG, "%s detected", desc);
    trigger.trigger();
    last_ms = now;
#ifdef USE_BINARY_SENSOR
    if (bs != nullptr) {
      static_cast<binary_sensor::BinarySensor *>(bs)->publish_state(true);
    }
#endif
  } else if (!state && now - last_ms > cooldown_ms && bs != nullptr) {
#ifdef USE_BINARY_SENSOR
    static_cast<binary_sensor::BinarySensor *>(bs)->publish_state(false);
#endif
  }
}

#ifdef USE_BINARY_SENSOR
#define BS_OPTIONAL_PTR(x) ((void *) (x))
#else
#define BS_OPTIONAL_PTR(x) (nullptr)
#endif

void LIS2DW12Component::process_events_() {
  uint32_t now = millis();

  binary_event_debounce(this->status_.all_int.single_tap, now, this->status_.last_tap_ms, this->tap_trigger_,
                        TAP_COOLDOWN_MS, BS_OPTIONAL_PTR(this->tap_binary_sensor_), "Tap");
  binary_event_debounce(this->status_.all_int.double_tap, now, this->status_.last_double_tap_ms,
                        this->double_tap_trigger_, DOUBLE_TAP_COOLDOWN_MS,
                        BS_OPTIONAL_PTR(this->double_tap_binary_sensor_), "Double Tap");
  binary_event_debounce(this->status_.all_int.ff_ia, now, this->status_.last_freefall_ms, this->freefall_trigger_,
                        FREEFALL_COOLDOWN_MS, BS_OPTIONAL_PTR(this->freefall_binary_sensor_), "Freefall");
  binary_event_debounce(this->status_.all_int.wu_ia, now, this->status_.last_active_ms, this->active_trigger_,
                        ACTIVITY_COOLDOWN_MS, BS_OPTIONAL_PTR(this->active_binary_sensor_), "Activity");

  if (this->status_.all_int.d6d_ia) {
    ESP_LOGVV(TAG, "Orientation changed");
    this->orientation_trigger_.trigger();
  }
}

void LIS2DW12Component::loop() {
  if (!this->is_ready()) {
    return;
  }

  if (!this->read_interrupt_status_()) {
    this->status_set_warning();
    return;
  }
  this->process_events_();
}

void LIS2DW12Component::update() {
  if (!this->is_ready()) {
    return;
  }

  if (!this->read_acceleration_data_()) {
    this->status_set_warning();
    return;
  }

  ESP_LOGV(TAG, "Acceleration: {x = %+1.3f m/s², y = %+1.3f m/s², z = %+1.3f m/s²}", this->data_.x, this->data_.y,
           this->data_.z);

#ifdef USE_SENSOR
  if (this->acceleration_x_sensor_ != nullptr)
    this->acceleration_x_sensor_->publish_state(this->data_.x);
  if (this->acceleration_y_sensor_ != nullptr)
    this->acceleration_y_sensor_->publish_state(this->data_.y);
  if (this->acceleration_z_sensor_ != nullptr)
    this->acceleration_z_sensor_->publish_state(this->data_.z);
#endif

#ifdef USE_TEXT_SENSOR
  if (this->orientation_text_sensor_ != nullptr &&
      (this->status_.sixd.raw != this->status_.sixd_old.raw || this->status_.never_published)) {
    this->orientation_text_sensor_->publish_state(this->get_orientation_string_());
    this->status_.sixd_old = this->status_.sixd;
  }
#endif

  this->status_.never_published = false;
  this->status_clear_warning();
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

}  // namespace esphome::lis2dw12_base
