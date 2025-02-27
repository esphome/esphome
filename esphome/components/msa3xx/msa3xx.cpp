#include "msa3xx.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace msa3xx {

static const char *const TAG = "msa3xx";

const uint8_t MSA_3XX_PART_ID = 0x13;

const float GRAVITY_EARTH = 9.80665f;
const float LSB_COEFF = 1000.0f / (GRAVITY_EARTH * 3.9);  // LSB to 1 LSB = 3.9mg = 0.0039g
const float G_OFFSET_MIN = -4.5f;  // -127...127 LSB = +- 0.4953g = +- 4.857 m/s^2 => +- 4.5 for the safe
const float G_OFFSET_MAX = 4.5f;   // -127...127 LSB = +- 0.4953g = +- 4.857 m/s^2 => +- 4.5 for the safe

const uint8_t RESOLUTION[] = {14, 12, 10, 8};

const uint32_t TAP_COOLDOWN_MS = 500;
const uint32_t DOUBLE_TAP_COOLDOWN_MS = 500;
const uint32_t ACTIVITY_COOLDOWN_MS = 500;

const char *model_to_string(Model model) {
  switch (model) {
    case Model::MSA301:
      return "MSA301";
    case Model::MSA311:
      return "MSA311";
    default:
      return "Unknown";
  }
}

const char *power_mode_to_string(PowerMode power_mode) {
  switch (power_mode) {
    case PowerMode::NORMAL:
      return "Normal";
    case PowerMode::LOW_POWER:
      return "Low Power";
    case PowerMode::SUSPEND:
      return "Suspend";
    default:
      return "Unknown";
  }
}

const char *res_to_string(Resolution resolution) {
  switch (resolution) {
    case Resolution::RES_14BIT:
      return "14-bit";
    case Resolution::RES_12BIT:
      return "12-bit";
    case Resolution::RES_10BIT:
      return "10-bit";
    case Resolution::RES_8BIT:
      return "8-bit";
    default:
      return "Unknown";
  }
}

const char *range_to_string(Range range) {
  switch (range) {
    case Range::RANGE_2G:
      return "±2g";
    case Range::RANGE_4G:
      return "±4g";
    case Range::RANGE_8G:
      return "±8g";
    case Range::RANGE_16G:
      return "±16g";
    default:
      return "Unknown";
  }
}

const char *bandwidth_to_string(Bandwidth bandwidth) {
  switch (bandwidth) {
    case Bandwidth::BW_1_95HZ:
      return "1.95 Hz";
    case Bandwidth::BW_3_9HZ:
      return "3.9 Hz";
    case Bandwidth::BW_7_81HZ:
      return "7.81 Hz";
    case Bandwidth::BW_15_63HZ:
      return "15.63 Hz";
    case Bandwidth::BW_31_25HZ:
      return "31.25 Hz";
    case Bandwidth::BW_62_5HZ:
      return "62.5 Hz";
    case Bandwidth::BW_125HZ:
      return "125 Hz";
    case Bandwidth::BW_250HZ:
      return "250 Hz";
    case Bandwidth::BW_500HZ:
      return "500 Hz";
    default:
      return "Unknown";
  }
}

const char *orientation_xy_to_string(OrientationXY orientation) {
  switch (orientation) {
    case OrientationXY::PORTRAIT_UPRIGHT:
      return "Portrait Upright";
    case OrientationXY::PORTRAIT_UPSIDE_DOWN:
      return "Portrait Upside Down";
    case OrientationXY::LANDSCAPE_LEFT:
      return "Landscape Left";
    case OrientationXY::LANDSCAPE_RIGHT:
      return "Landscape Right";
    default:
      return "Unknown";
  }
}

const char *orientation_z_to_string(bool orientation) { return orientation ? "Downwards looking" : "Upwards looking"; }

void MSA3xxComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MSA3xx...");

  uint8_t part_id{0xff};
  if (!this->read_byte(static_cast<uint8_t>(RegisterMap::PART_ID), &part_id) || (part_id != MSA_3XX_PART_ID)) {
    ESP_LOGE(TAG, "Part ID is wrong or missing. Got 0x%02X", part_id);
    this->mark_failed();
    return;
  }

  // Resolution LSB/g
  // Range : MSA301      : MSA311
  // S2g   : 1024 (2^10) : 4096 (2^12)
  // S4g   : 512  (2^9)  : 2048 (2^11)
  // S8g   : 256  (2^8)  : 1024 (2^10)
  // S16g  : 128  (2^7)  : 512  (2^9)
  if (this->model_ == Model::MSA301) {
    this->device_params_.accel_data_width = 14;
    this->device_params_.scale_factor_exp = static_cast<uint8_t>(this->range_) - 12;
  } else if (this->model_ == Model::MSA311) {
    this->device_params_.accel_data_width = 12;
    this->device_params_.scale_factor_exp = static_cast<uint8_t>(this->range_) - 10;
  } else {
    ESP_LOGE(TAG, "Unknown model");
    this->mark_failed();
    return;
  }

  this->setup_odr_(this->data_rate_);
  this->setup_power_mode_bandwidth_(this->power_mode_, this->bandwidth_);
  this->setup_range_resolution_(this->range_, this->resolution_);                       // 2g...16g, 14...8 bit
  this->setup_offset_(this->offset_x_, this->offset_y_, this->offset_z_);               // calibration offsets
  this->write_byte(static_cast<uint8_t>(RegisterMap::TAP_DURATION), 0b11000100);        // set tap duration 250ms
  this->write_byte(static_cast<uint8_t>(RegisterMap::SWAP_POLARITY), this->swap_.raw);  // set axes polarity
  this->write_byte(static_cast<uint8_t>(RegisterMap::INT_SET_0), 0b01110111);           // enable all interrupts
  this->write_byte(static_cast<uint8_t>(RegisterMap::INT_SET_1), 0b00011000);           // including orientation
}

void MSA3xxComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MSA3xx:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with MSA3xx failed!");
  }
  ESP_LOGCONFIG(TAG, "  Model: %s", model_to_string(this->model_));
  ESP_LOGCONFIG(TAG, "  Power Mode: %s", power_mode_to_string(this->power_mode_));
  ESP_LOGCONFIG(TAG, "  Bandwidth: %s", bandwidth_to_string(this->bandwidth_));
  ESP_LOGCONFIG(TAG, "  Range: %s", range_to_string(this->range_));
  ESP_LOGCONFIG(TAG, "  Resolution: %s", res_to_string(this->resolution_));
  ESP_LOGCONFIG(TAG, "  Offsets: {%.3f m/s², %.3f m/s², %.3f m/s²}", this->offset_x_, this->offset_y_, this->offset_z_);
  ESP_LOGCONFIG(TAG, "  Transform: {mirror_x=%s, mirror_y=%s, mirror_z=%s, swap_xy=%s}", YESNO(this->swap_.x_polarity),
                YESNO(this->swap_.y_polarity), YESNO(this->swap_.z_polarity), YESNO(this->swap_.x_y_swap));
  LOG_UPDATE_INTERVAL(this);

#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Tap", this->tap_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Double Tap", this->double_tap_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Active", this->active_binary_sensor_);
#endif

#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Acceleration X", this->acceleration_x_sensor_);
  LOG_SENSOR("  ", "Acceleration Y", this->acceleration_y_sensor_);
  LOG_SENSOR("  ", "Acceleration Z", this->acceleration_z_sensor_);
#endif

#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "Orientation XY", this->orientation_xy_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Orientation Z", this->orientation_z_text_sensor_);
#endif
}

bool MSA3xxComponent::read_data_() {
  uint8_t accel_data[6];
  if (!this->read_bytes(static_cast<uint8_t>(RegisterMap::ACC_X_LSB), accel_data, 6)) {
    return false;
  }

  auto raw_to_x_bit = [](uint16_t lsb, uint16_t msb, uint8_t data_bits) -> uint16_t {
    return ((msb << 8) | lsb) >> (16 - data_bits);
  };

  auto lpf = [](float new_value, float old_value, float alpha = 0.5f) {
    return alpha * new_value + (1.0f - alpha) * old_value;
  };

  this->data_.lsb_x =
      this->twos_complement_(raw_to_x_bit(accel_data[0], accel_data[1], this->device_params_.accel_data_width),
                             this->device_params_.accel_data_width);
  this->data_.lsb_y =
      this->twos_complement_(raw_to_x_bit(accel_data[2], accel_data[3], this->device_params_.accel_data_width),
                             this->device_params_.accel_data_width);
  this->data_.lsb_z =
      this->twos_complement_(raw_to_x_bit(accel_data[4], accel_data[5], this->device_params_.accel_data_width),
                             this->device_params_.accel_data_width);

  this->data_.x = lpf(ldexp(this->data_.lsb_x, this->device_params_.scale_factor_exp) * GRAVITY_EARTH, this->data_.x);
  this->data_.y = lpf(ldexp(this->data_.lsb_y, this->device_params_.scale_factor_exp) * GRAVITY_EARTH, this->data_.y);
  this->data_.z = lpf(ldexp(this->data_.lsb_z, this->device_params_.scale_factor_exp) * GRAVITY_EARTH, this->data_.z);

  return true;
}

bool MSA3xxComponent::read_motion_status_() {
  if (!this->read_byte(static_cast<uint8_t>(RegisterMap::MOTION_INTERRUPT), &this->status_.motion_int.raw)) {
    return false;
  }

  if (!this->read_byte(static_cast<uint8_t>(RegisterMap::ORIENTATION_STATUS), &this->status_.orientation.raw)) {
    return false;
  }

  return true;
}

void MSA3xxComponent::loop() {
  if (!this->is_ready()) {
    return;
  }

  RegMotionInterrupt old_motion_int = this->status_.motion_int;

  if (!this->read_data_() || !this->read_motion_status_()) {
    this->status_set_warning();
    return;
  }

  this->process_motions_(old_motion_int);
}

void MSA3xxComponent::update() {
  ESP_LOGV(TAG, "Updating MSA3xx...");

  if (!this->is_ready()) {
    ESP_LOGV(TAG, "Component MSA3xx not ready for update");
    return;
  }
  ESP_LOGV(TAG, "Acceleration: {x = %+1.3f m/s², y = %+1.3f m/s², z = %+1.3f m/s²}; ", this->data_.x, this->data_.y,
           this->data_.z);

  ESP_LOGV(TAG, "Orientation: {XY = %s, Z = %s}", orientation_xy_to_string(this->status_.orientation.orient_xy),
           orientation_z_to_string(this->status_.orientation.orient_z));

#ifdef USE_SENSOR
  if (this->acceleration_x_sensor_ != nullptr)
    this->acceleration_x_sensor_->publish_state(this->data_.x);
  if (this->acceleration_y_sensor_ != nullptr)
    this->acceleration_y_sensor_->publish_state(this->data_.y);
  if (this->acceleration_z_sensor_ != nullptr)
    this->acceleration_z_sensor_->publish_state(this->data_.z);
#endif

#ifdef USE_TEXT_SENSOR
  if (this->orientation_xy_text_sensor_ != nullptr &&
      (this->status_.orientation.orient_xy != this->status_.orientation_old.orient_xy ||
       this->status_.never_published)) {
    this->orientation_xy_text_sensor_->publish_state(orientation_xy_to_string(this->status_.orientation.orient_xy));
  }
  if (this->orientation_z_text_sensor_ != nullptr &&
      (this->status_.orientation.orient_z != this->status_.orientation_old.orient_z || this->status_.never_published)) {
    this->orientation_z_text_sensor_->publish_state(orientation_z_to_string(this->status_.orientation.orient_z));
  }
  this->status_.orientation_old = this->status_.orientation;
#endif

  this->status_.never_published = false;
  this->status_clear_warning();
}
float MSA3xxComponent::get_setup_priority() const { return setup_priority::DATA; }

void MSA3xxComponent::set_offset(float offset_x, float offset_y, float offset_z) {
  this->offset_x_ = offset_x;
  this->offset_y_ = offset_y;
  this->offset_z_ = offset_z;
}

void MSA3xxComponent::set_transform(bool mirror_x, bool mirror_y, bool mirror_z, bool swap_xy) {
  this->swap_.x_polarity = mirror_x;
  this->swap_.y_polarity = mirror_y;
  this->swap_.z_polarity = mirror_z;
  this->swap_.x_y_swap = swap_xy;
}

void MSA3xxComponent::setup_odr_(DataRate rate) {
  RegOutputDataRate reg_odr;
  auto reg = this->read_byte(static_cast<uint8_t>(RegisterMap::ODR));
  if (reg.has_value()) {
    reg_odr.raw = reg.value();
  } else {
    reg_odr.raw = 0x0F;  // defaut from datasheet
  }

  reg_odr.x_axis_disable = false;
  reg_odr.y_axis_disable = false;
  reg_odr.z_axis_disable = false;
  reg_odr.odr = rate;

  this->write_byte(static_cast<uint8_t>(RegisterMap::ODR), reg_odr.raw);
}

void MSA3xxComponent::setup_power_mode_bandwidth_(PowerMode power_mode, Bandwidth bandwidth) {
  // 0x11 POWER_MODE_BANDWIDTH
  auto reg = this->read_byte(static_cast<uint8_t>(RegisterMap::POWER_MODE_BANDWIDTH));

  RegPowerModeBandwidth power_mode_bandwidth;
  if (reg.has_value()) {
    power_mode_bandwidth.raw = reg.value();
  } else {
    power_mode_bandwidth.raw = 0xde;  // defaut from datasheet
  }

  power_mode_bandwidth.power_mode = power_mode;
  power_mode_bandwidth.low_power_bandwidth = bandwidth;

  this->write_byte(static_cast<uint8_t>(RegisterMap::POWER_MODE_BANDWIDTH), power_mode_bandwidth.raw);
}

void MSA3xxComponent::setup_range_resolution_(Range range, Resolution resolution) {
  RegRangeResolution reg;
  reg.raw = this->read_byte(static_cast<uint8_t>(RegisterMap::RANGE_RESOLUTION)).value_or(0x00);
  reg.range = range;
  if (this->model_ == Model::MSA301) {
    reg.resolution = resolution;
  }
  this->write_byte(static_cast<uint8_t>(RegisterMap::RANGE_RESOLUTION), reg.raw);
}

void MSA3xxComponent::setup_offset_(float offset_x, float offset_y, float offset_z) {
  uint8_t offset[3];

  auto offset_g_to_lsb = [](float accel) -> int8_t {
    float acccel_clamped = clamp(accel, G_OFFSET_MIN, G_OFFSET_MAX);
    return static_cast<int8_t>(acccel_clamped * LSB_COEFF);
  };

  offset[0] = offset_g_to_lsb(offset_x);
  offset[1] = offset_g_to_lsb(offset_y);
  offset[2] = offset_g_to_lsb(offset_z);

  ESP_LOGV(TAG, "Offset (%.3f, %.3f, %.3f)=>LSB(%d, %d, %d)", offset_x, offset_y, offset_z, offset[0], offset[1],
           offset[2]);

  this->write_bytes(static_cast<uint8_t>(RegisterMap::OFFSET_COMP_X), (uint8_t *) &offset, 3);
}

int64_t MSA3xxComponent::twos_complement_(uint64_t value, uint8_t bits) {
  if (value > (1ULL << (bits - 1))) {
    return (int64_t) (value - (1ULL << bits));
  } else {
    return (int64_t) value;
  }
}

void binary_event_debounce(bool state, bool old_state, uint32_t now, uint32_t &last_ms, Trigger<> &trigger,
                           uint32_t cooldown_ms, void *bs, const char *desc) {
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

void MSA3xxComponent::process_motions_(RegMotionInterrupt old) {
  uint32_t now = millis();

  binary_event_debounce(this->status_.motion_int.single_tap_interrupt, old.single_tap_interrupt, now,
                        this->status_.last_tap_ms, this->tap_trigger_, TAP_COOLDOWN_MS,
                        BS_OPTIONAL_PTR(this->tap_binary_sensor_), "Tap");
  binary_event_debounce(this->status_.motion_int.double_tap_interrupt, old.double_tap_interrupt, now,
                        this->status_.last_double_tap_ms, this->double_tap_trigger_, DOUBLE_TAP_COOLDOWN_MS,
                        BS_OPTIONAL_PTR(this->double_tap_binary_sensor_), "Double Tap");
  binary_event_debounce(this->status_.motion_int.active_interrupt, old.active_interrupt, now,
                        this->status_.last_action_ms, this->active_trigger_, ACTIVITY_COOLDOWN_MS,
                        BS_OPTIONAL_PTR(this->active_binary_sensor_), "Activity");

  if (this->status_.motion_int.orientation_interrupt) {
    ESP_LOGVV(TAG, "Orientation changed");
    this->orientation_trigger_.trigger();
  }
}

}  // namespace msa3xx
}  // namespace esphome
