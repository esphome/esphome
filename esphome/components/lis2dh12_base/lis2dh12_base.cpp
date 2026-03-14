#include "lis2dh12_base.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::lis2dh12_base {

static const char *const TAG = "lis2dh12";

static const uint8_t LIS2DH12_WHO_AM_I_VALUE = 0x33;
static const float GRAVITY_EARTH = 9.80665f;

static const uint32_t TAP_COOLDOWN_MS = 500;
static const uint32_t DOUBLE_TAP_COOLDOWN_MS = 500;
static const uint32_t FREEFALL_COOLDOWN_MS = 500;
static const uint32_t ACTIVITY_COOLDOWN_MS = 500;

// Sensitivity in mg/LSB indexed by Range enum value: 2G=0, 4G=1, 8G=2, 16G=3
// High-resolution mode (12-bit)
static const float SENSITIVITY_HR[] = {1.0f, 2.0f, 4.0f, 12.0f};
// Normal mode (10-bit)
static const float SENSITIVITY_NM[] = {4.0f, 8.0f, 16.0f, 48.0f};
// Low-power mode (8-bit)
static const float SENSITIVITY_LP[] = {16.0f, 32.0f, 64.0f, 192.0f};

static const LogString *resolution_to_string(Resolution res) {
  switch (res) {
    case Resolution::HIGH_RESOLUTION:
      return LOG_STR("High Resolution (12-bit)");
    case Resolution::NORMAL:
      return LOG_STR("Normal (10-bit)");
    case Resolution::LOW_POWER:
      return LOG_STR("Low Power (8-bit)");
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
    case DataRate::RATE_1HZ:
      return LOG_STR("1 Hz");
    case DataRate::RATE_10HZ:
      return LOG_STR("10 Hz");
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
    case DataRate::RATE_1620HZ_LP:
      return LOG_STR("1620 Hz (LP)");
    case DataRate::RATE_5376HZ_LP:
      return LOG_STR("5376 Hz (LP) / 1344 Hz (NM/HR)");
    default:
      return LOG_STR("Unknown");
  }
}

void LIS2DH12Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up LIS2DH12...");

  // Verify WHO_AM_I
  uint8_t who_am_i{0};
  if (!this->read_byte(static_cast<uint8_t>(RegisterMap::WHO_AM_I), &who_am_i) || who_am_i != LIS2DH12_WHO_AM_I_VALUE) {
    ESP_LOGE(TAG, "WHO_AM_I check failed. Got 0x%02X, expected 0x%02X", who_am_i, LIS2DH12_WHO_AM_I_VALUE);
    this->mark_failed();
    return;
  }

  // Reboot memory content (CTRL_REG5 bit 7)
  if (!this->write_byte(static_cast<uint8_t>(RegisterMap::CTRL_REG5), 0x80)) {
    ESP_LOGE(TAG, "Reboot failed");
    this->mark_failed();
    return;
  }
  delay(5);  // NOLINT - wait for reboot to complete

  if (!this->configure_registers_()) {
    ESP_LOGE(TAG, "Register configuration failed");
    this->mark_failed();
    return;
  }

  // Set sensitivity based on range and resolution
  uint8_t range_idx = static_cast<uint8_t>(this->range_);
  switch (this->resolution_) {
    case Resolution::HIGH_RESOLUTION:
      this->sensitivity_ = SENSITIVITY_HR[range_idx];
      break;
    case Resolution::NORMAL:
      this->sensitivity_ = SENSITIVITY_NM[range_idx];
      break;
    case Resolution::LOW_POWER:
      this->sensitivity_ = SENSITIVITY_LP[range_idx];
      break;
  }
}

bool LIS2DH12Component::configure_registers_() {
  // CTRL_REG1: ODR[7:4] + LPen[3] + Zen[2] + Yen[1] + Xen[0]
  uint8_t ctrl1 = (static_cast<uint8_t>(this->data_rate_) << 4) | 0x07;  // all axes enabled
  if (this->resolution_ == Resolution::LOW_POWER) {
    ctrl1 |= 0x08;  // LPen=1
  }
  if (!this->write_byte(static_cast<uint8_t>(RegisterMap::CTRL_REG1), ctrl1)) {
    return false;
  }

  // CTRL_REG3: I1_CLICK=1 (bit 7), I1_IA1=1 (bit 6)
  if (!this->write_byte(static_cast<uint8_t>(RegisterMap::CTRL_REG3), 0xC0)) {
    return false;
  }

  // CTRL_REG4: BDU[7]=1 + FS[5:4] + HR[3]
  uint8_t ctrl4 = 0x80;  // BDU=1
  ctrl4 |= (static_cast<uint8_t>(this->range_) << 4);
  if (this->resolution_ == Resolution::HIGH_RESOLUTION) {
    ctrl4 |= 0x08;  // HR=1
  }
  if (!this->write_byte(static_cast<uint8_t>(RegisterMap::CTRL_REG4), ctrl4)) {
    return false;
  }

  // CTRL_REG5: LIR_INT1=1 (bit 3) - latch interrupt on INT1
  if (!this->write_byte(static_cast<uint8_t>(RegisterMap::CTRL_REG5), 0x08)) {
    return false;
  }

  // CLICK_CFG: XS=1, XD=1, YS=1, YD=1, ZS=1, ZD=1 (all axes, single+double)
  if (!this->write_byte(static_cast<uint8_t>(RegisterMap::CLICK_CFG), 0x3F)) {
    return false;
  }

  // CLICK_THS: LIR_CLICK[7]=1, THS[6:0]=0x30
  if (!this->write_byte(static_cast<uint8_t>(RegisterMap::CLICK_THS), 0xB0)) {
    return false;
  }

  // TIME_LIMIT
  if (!this->write_byte(static_cast<uint8_t>(RegisterMap::TIME_LIMIT), 0x10)) {
    return false;
  }

  // TIME_LATENCY
  if (!this->write_byte(static_cast<uint8_t>(RegisterMap::TIME_LATENCY), 0x20)) {
    return false;
  }

  // TIME_WINDOW
  if (!this->write_byte(static_cast<uint8_t>(RegisterMap::TIME_WINDOW), 0x40)) {
    return false;
  }

  // INT1_CFG: 6D=1, ZHIE=1, ZLIE=1, YHIE=1, YLIE=1, XHIE=1, XLIE=1
  if (!this->write_byte(static_cast<uint8_t>(RegisterMap::INT1_CFG), 0x7F)) {
    return false;
  }

  // INT1_THS
  if (!this->write_byte(static_cast<uint8_t>(RegisterMap::INT1_THS), 0x10)) {
    return false;
  }

  // INT1_DURATION
  if (!this->write_byte(static_cast<uint8_t>(RegisterMap::INT1_DURATION), 0x00)) {
    return false;
  }

  // ACT_THS
  if (!this->write_byte(static_cast<uint8_t>(RegisterMap::ACT_THS), 0x04)) {
    return false;
  }

  // ACT_DUR
  if (!this->write_byte(static_cast<uint8_t>(RegisterMap::ACT_DUR), 0x02)) {
    return false;
  }

  return true;
}

void LIS2DH12Component::dump_config() {
  ESP_LOGCONFIG(TAG, "LIS2DH12:");
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
  ESP_LOGCONFIG(TAG,
                "  Resolution: %s\n"
                "  Data Rate: %s\n"
                "  Range: %s\n"
                "  Offsets: {%.3f m/s\xC2\xB2, %.3f m/s\xC2\xB2, %.3f m/s\xC2\xB2}\n"
                "  Transform: {mirror_x=%s, mirror_y=%s, mirror_z=%s, swap_xy=%s}",
                LOG_STR_ARG(resolution_to_string(this->resolution_)),
                LOG_STR_ARG(data_rate_to_string(this->data_rate_)), LOG_STR_ARG(range_to_string(this->range_)),
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

bool LIS2DH12Component::read_acceleration_data_() {
  uint8_t accel_data[6];
  if (!this->read_bytes(static_cast<uint8_t>(RegisterMap::OUT_X_L), accel_data, 6)) {
    return false;
  }

  int16_t raw_x = static_cast<int16_t>((accel_data[1] << 8) | accel_data[0]);
  int16_t raw_y = static_cast<int16_t>((accel_data[3] << 8) | accel_data[2]);
  int16_t raw_z = static_cast<int16_t>((accel_data[5] << 8) | accel_data[4]);

  switch (this->resolution_) {
    case Resolution::HIGH_RESOLUTION:
      raw_x >>= 4;
      raw_y >>= 4;
      raw_z >>= 4;
      break;
    case Resolution::NORMAL:
      raw_x >>= 6;
      raw_y >>= 6;
      raw_z >>= 6;
      break;
    case Resolution::LOW_POWER:
      raw_x >>= 8;
      raw_y >>= 8;
      raw_z >>= 8;
      break;
  }

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

  float factor = this->sensitivity_ * 0.001f * GRAVITY_EARTH;
  this->data_.x = raw_x * factor + this->offset_x_;
  this->data_.y = raw_y * factor + this->offset_y_;
  this->data_.z = raw_z * factor + this->offset_z_;

  return true;
}

bool LIS2DH12Component::read_interrupt_status_() {
  uint8_t click_src = 0;
  uint8_t int1_src = 0;
  bool ok = this->read_byte(static_cast<uint8_t>(RegisterMap::CLICK_SRC), &click_src);
  ok = this->read_byte(static_cast<uint8_t>(RegisterMap::INT1_SRC), &int1_src) && ok;
  if (!ok) {
    this->status_.click.raw = 0;
    this->status_.int1.raw = 0;
    return false;
  }
  this->status_.click.raw = click_src;
  this->status_.int1.raw = int1_src;
  return true;
}

const char *LIS2DH12Component::get_orientation_string_() {
  if (this->status_.int1.xh)
    return "X Up";
  if (this->status_.int1.xl)
    return "X Down";
  if (this->status_.int1.yh)
    return "Y Up";
  if (this->status_.int1.yl)
    return "Y Down";
  if (this->status_.int1.zh)
    return "Z Up";
  if (this->status_.int1.zl)
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

void LIS2DH12Component::process_events_() {
  uint32_t now = millis();

  binary_event_debounce(this->status_.click.sclick, now, this->status_.last_tap_ms, this->tap_trigger_, TAP_COOLDOWN_MS,
                        BS_OPTIONAL_PTR(this->tap_binary_sensor_), "Tap");
  binary_event_debounce(this->status_.click.dclick, now, this->status_.last_double_tap_ms, this->double_tap_trigger_,
                        DOUBLE_TAP_COOLDOWN_MS, BS_OPTIONAL_PTR(this->double_tap_binary_sensor_), "Double Tap");

  binary_event_debounce(
      this->status_.int1.ia && !this->status_.int1.xh && !this->status_.int1.yh && !this->status_.int1.zh, now,
      this->status_.last_freefall_ms, this->freefall_trigger_, FREEFALL_COOLDOWN_MS,
      BS_OPTIONAL_PTR(this->freefall_binary_sensor_), "Freefall");
  binary_event_debounce(
      this->status_.int1.ia && (this->status_.int1.xh || this->status_.int1.yh || this->status_.int1.zh), now,
      this->status_.last_active_ms, this->active_trigger_, ACTIVITY_COOLDOWN_MS,
      BS_OPTIONAL_PTR(this->active_binary_sensor_), "Activity");

  if (this->status_.int1.ia && this->status_.int1.raw != this->status_.int1_old.raw) {
    ESP_LOGVV(TAG, "Orientation changed");
    this->orientation_trigger_.trigger();
  }
}

void LIS2DH12Component::loop() {
  if (!this->is_ready()) {
    return;
  }

  if (!this->read_interrupt_status_()) {
    this->status_set_warning();
    return;
  }
  this->process_events_();
}

void LIS2DH12Component::update() {
  if (!this->is_ready()) {
    return;
  }

  if (!this->read_acceleration_data_()) {
    this->status_set_warning();
    return;
  }

  ESP_LOGV(TAG, "Acceleration: {x = %+1.3f m/s\xC2\xB2, y = %+1.3f m/s\xC2\xB2, z = %+1.3f m/s\xC2\xB2}", this->data_.x,
           this->data_.y, this->data_.z);

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
      (this->status_.int1.raw != this->status_.int1_old.raw || this->status_.never_published)) {
    this->orientation_text_sensor_->publish_state(this->get_orientation_string_());
    this->status_.int1_old = this->status_.int1;
  }
#endif

  this->status_.never_published = false;
  this->status_clear_warning();
}

void LIS2DH12Component::set_offset(float offset_x, float offset_y, float offset_z) {
  this->offset_x_ = offset_x;
  this->offset_y_ = offset_y;
  this->offset_z_ = offset_z;
}

void LIS2DH12Component::set_transform(bool mirror_x, bool mirror_y, bool mirror_z, bool swap_xy) {
  this->mirror_x_ = mirror_x;
  this->mirror_y_ = mirror_y;
  this->mirror_z_ = mirror_z;
  this->swap_xy_ = swap_xy;
}

}  // namespace esphome::lis2dh12_base
