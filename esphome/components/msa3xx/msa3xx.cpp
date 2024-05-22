#include "msa3xx.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace msa3xx {

static const char *const TAG = "msa3xx";

const uint8_t MSA3xx_PART_ID = 0x13;
const float GRAVITY_EARTH = 9.80665f;

const uint8_t RESOLUTION[] = {14, 12, 10, 8};

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

const char *res_to_string(Resolution Resolution) {
  switch (Resolution) {
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

void MSA3xxComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MSA3xx...");

  uint8_t part_id{0xff};
  if (!this->read_byte(static_cast<uint8_t>(RegisterMap::PART_ID), &part_id) || (part_id != MSA3xx_PART_ID)) {
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
    this->params_.accel_data_width = 14;
    this->params_.scale_factor_exp = static_cast<uint8_t>(this->range_) - 12;
  } else if (this->model_ == Model::MSA311) {
    this->params_.accel_data_width = 12;
    this->params_.scale_factor_exp = static_cast<uint8_t>(this->range_) - 10;

  } else {
    ESP_LOGE(TAG, "Unknown model");
    this->mark_failed();
    return;
  }

  this->setup_odr_();
  this->setup_power_mode_bandwidth_(this->power_mode_, this->bandwidth_);
  this->setup_range_resolution_(this->range_, this->resolution_);
  this->setup_offset_(this->offset_x_, this->offset_y_, this->offset_z_);

  this->write_byte(static_cast<uint8_t>(RegisterMap::INT_SET_0), this->int_set_0_);
  this->write_byte(static_cast<uint8_t>(RegisterMap::INT_SET_1), this->int_set_1_);
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
  ESP_LOGCONFIG(TAG, "  Offsets: (%.3f m/s², %.3f m/s², %.3f m/s²)", this->offset_x_, this->offset_y_, this->offset_z_);
  LOG_UPDATE_INTERVAL(this);

#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Acceleration X", this->accel_x_sensor_);
  LOG_SENSOR("  ", "Acceleration Y", this->accel_y_sensor_);
  LOG_SENSOR("  ", "Acceleration Z", this->accel_z_sensor_);
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

  this->data_.lsb_x = this->two_complement_to_normal_(
      raw_to_x_bit(accel_data[0], accel_data[1], this->params_.accel_data_width), this->params_.accel_data_width);
  this->data_.lsb_y = this->two_complement_to_normal_(
      raw_to_x_bit(accel_data[2], accel_data[3], this->params_.accel_data_width), this->params_.accel_data_width);
  this->data_.lsb_z = this->two_complement_to_normal_(
      raw_to_x_bit(accel_data[4], accel_data[5], this->params_.accel_data_width), this->params_.accel_data_width);

  this->data_.x = lpf(ldexp(this->data_.lsb_x, this->params_.scale_factor_exp) * GRAVITY_EARTH, this->data_.x);
  this->data_.y = lpf(ldexp(this->data_.lsb_y, this->params_.scale_factor_exp) * GRAVITY_EARTH, this->data_.y);
  this->data_.z = lpf(ldexp(this->data_.lsb_z, this->params_.scale_factor_exp) * GRAVITY_EARTH, this->data_.z);

  // ESP_LOGVV(TAG, "Got raw data {x=%5d, y=%5d, z=%5d}, accel={x=%+1.3f m/s², y=%+1.3f m/s², z=%+1.3f m/s²}",
  //          this->data_.lsb_x, this->data_.lsb_y, this->data_.lsb_z, this->data_.x, this->data_.y, this->data_.z);

  return true;
}

bool MSA3xxComponent::read_motion_status_() {
  if (!this->read_byte(static_cast<uint8_t>(RegisterMap::MOTION_INTERRUPT), &this->status_.mi.raw)) {
    return false;
  }

  return true;
}

void MSA3xxComponent::loop() {
  if (!this->is_ready()) {
    return;
  }

  if (!this->read_data_() || !this->read_motion_status_()) {
    this->status_set_warning();
    return;
  }

  this->process_interrupts_();
}

void MSA3xxComponent::update() {
  ESP_LOGV(TAG, "Updating MSA3xx...");

  if (!this->is_ready()) {
    ESP_LOGV(TAG, "Component MSA3xx not ready for update");
    return;
  }
  ESP_LOGV(TAG, "Acceleration: {x = %+1.3f m/s², y = %+1.3f m/s², z = %+1.3f m/s²}", this->data_.x, this->data_.y,
           this->data_.z);

#ifdef USE_SENSOR
  if (this->accel_x_sensor_ != nullptr)
    this->accel_x_sensor_->publish_state(this->data_.x);
  if (this->accel_y_sensor_ != nullptr)
    this->accel_y_sensor_->publish_state(this->data_.y);
  if (this->accel_z_sensor_ != nullptr)
    this->accel_z_sensor_->publish_state(this->data_.z);
#endif

  this->status_clear_warning();
}
float MSA3xxComponent::get_setup_priority() const { return setup_priority::DATA; }

void MSA3xxComponent::set_offset(float offset_x, float offset_y, float offset_z) {
  this->offset_x_ = offset_x;
  this->offset_y_ = offset_y;
  this->offset_z_ = offset_z;
}

void MSA3xxComponent::setup_odr_() {
  RegODR odr;
  auto reg = this->read_byte(static_cast<uint8_t>(RegisterMap::ODR));
  if (reg.has_value()) {
    odr.raw = reg.value();
  } else {
    odr.raw = 0x0F;  // defaut from datasheet
  }

  odr.x_axis_disable = 0;
  odr.y_axis_disable = 0;
  odr.z_axis_disable = 0;

  this->write_byte(static_cast<uint8_t>(RegisterMap::ODR), odr.raw);
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
  power_mode_bandwidth.low_power_bw = bandwidth;

  this->write_byte(static_cast<uint8_t>(RegisterMap::POWER_MODE_BANDWIDTH), power_mode_bandwidth.raw);
}

void MSA3xxComponent::setup_range_resolution_(Range range, Resolution resolution) {
  RegRangeResolution reg;
  reg.raw = this->read_byte(static_cast<uint8_t>(RegisterMap::RANGE_RESOLUTION)).value_or(0x00);
  reg.range = range;
  reg.resolution = this->model_ == Model::MSA301 ? resolution : Resolution::RES_14BIT;
  this->write_byte(static_cast<uint8_t>(RegisterMap::RANGE_RESOLUTION), reg.raw);
}

void MSA3xxComponent::setup_offset_(float offset_x, float offset_y, float offset_z) {
  uint8_t offset[3];

  auto offset_g_to_lsb = [](float accel) -> int8_t {
    // 1 LSB = 3.9mg = 0.0039g
    // -127...127 LSB = +- 0.4953g = +- 4.857 m/s^2 => +- 4.5 for the safe
    float acccel_clamped = clamp(accel, -4.5f, 4.5f);
    return static_cast<int8_t>(accel * 1000 / (GRAVITY_EARTH * 3.9));
  };
  offset[0] = offset_g_to_lsb(offset_x);
  offset[1] = offset_g_to_lsb(offset_y);
  offset[2] = offset_g_to_lsb(offset_z);

  ESP_LOGV(TAG, "Offset (%.3f, %.3f, %.3f)=>LSB(%d, %d, %d)", offset_x, offset_y, offset_z, offset[0], offset[1],
           offset[2]);

  this->write_bytes(static_cast<uint8_t>(RegisterMap::OFFSET_COMP_X), (uint8_t *) &offset, 3);
}

int64_t MSA3xxComponent::two_complement_to_normal_(uint64_t value, uint8_t bits) {
  if (value > (1ULL << (bits - 1))) {
    return (int64_t) (value - (1ULL << bits));
  } else {
    return (int64_t) value;
  }
}

void MSA3xxComponent::process_interrupts_() {
  if (this->status_.mi.raw == 0) {
    return;
  }

  if (this->status_.mi.single_tap_interrupt) {
    ESP_LOGW(TAG, "Single Tap detected");
    this->tap_trigger_.trigger();
  }

  if (this->status_.mi.double_tap_interrupt) {
    ESP_LOGW(TAG, "Double Tap detected");
    this->double_tap_trigger_.trigger();
  }

  if (this->status_.mi.orientation_interrupt) {
    ESP_LOGW(TAG, "Orientation changed");
    this->orientation_trigger_.trigger();
  }
}

}  // namespace msa3xx
}  // namespace esphome
