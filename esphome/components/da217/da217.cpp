#include "da217.h"
#include "esphome/core/log.h"

namespace esphome {
namespace da217 {

static const char *const TAG = "da217";

const uint8_t DA217_REGISTER_SPI_CONFIG = 0x00;
const uint8_t DA217_SPI_CONFIG_SDO_ACTIVE = 0b10000001;
const uint8_t DA217_SPI_CONFIG_LSB_FIRST = 0b01000010;
const uint8_t DA217_SPI_CONFIG_SOFT_RESET = 0b00100100;

const uint8_t DA217_REGISTER_CHIP_ID = 0x01;

const uint8_t DA217_REGISTER_ACC_X_LSB = 0x02;
const uint8_t DA217_REGISTER_ACC_X_MSB = 0x03;
const uint8_t DA217_REGISTER_ACC_Y_LSB = 0x04;
const uint8_t DA217_REGISTER_ACC_Y_MSB = 0x05;
const uint8_t DA217_REGISTER_ACC_Z_LSB = 0x06;
const uint8_t DA217_REGISTER_ACC_Z_MSB = 0x07;

const uint8_t DA217_REGISTER_RESOLUTION_RANGE = 0x0F;
const uint8_t DA217_RESOLUTION_RANGE_HIGH_PASS_FILTER = 1 << 7;
const uint8_t DA217_RESOLUTION_RANGE_WATCHDOG = 1 << 6;
const uint8_t DA217_RESOLUTION_RANGE_WATCHDOG_1MS = 0 << 5;
const uint8_t DA217_RESOLUTION_RANGE_WATCHDOG_50MS = 1 << 5;
const uint8_t DA217_RESOLUTION_RANGE_RESOLUTION_14BITS = 0b00 << 2;
const uint8_t DA217_RESOLUTION_RANGE_RESOLUTION_12BITS = 0b01 << 2;
const uint8_t DA217_RESOLUTION_RANGE_RESOLUTION_10BITS = 0b10 << 2;
const uint8_t DA217_RESOLUTION_RANGE_RESOLUTION_8BITS = 0b11 << 2;
const uint8_t DA217_RESOLUTION_RANGE_SCALE_2G = 0b00;
const uint8_t DA217_RESOLUTION_RANGE_SCALE_4G = 0b01;
const uint8_t DA217_RESOLUTION_RANGE_SCALE_8G = 0b10;
const uint8_t DA217_RESOLUTION_RANGE_SCALE_16G = 0b11;

const uint8_t DA217_REGISTER_ODR_AXIS = 0x10;
const uint8_t DA217_ODR_AXIS_DISABLE_X = 1 << 7;
const uint8_t DA217_ODR_AXIS_DISABLE_Y = 1 << 6;
const uint8_t DA217_ODR_AXIS_DISABLE_Z = 1 << 5;
const uint8_t DA217_ODR_AXIS_1HZ = 0b0000;
const uint8_t DA217_ODR_AXIS_1_95HZ = 0b0001;
const uint8_t DA217_ODR_AXIS_3_9HZ = 0b0010;
const uint8_t DA217_ODR_AXIS_7_81HZ = 0b0011;
const uint8_t DA217_ODR_AXIS_15_63HZ = 0b0100;
const uint8_t DA217_ODR_AXIS_31_25HZ = 0b0101;
const uint8_t DA217_ODR_AXIS_62_5HZ = 0b0110;
const uint8_t DA217_ODR_AXIS_125HZ = 0b0111;
const uint8_t DA217_ODR_AXIS_250HZ = 0b1000;
const uint8_t DA217_ODR_AXIS_500HZ = 0b1001;

const uint8_t DA217_REGISTER_MODE_BW = 0x11;
const uint8_t DA217_MODE_BW_SUSPEND_MODE = 1 << 7;
const uint8_t DA217_MODE_BW_NORMAL_MODE = 0 << 7;

const uint8_t DA217_REGISTER_INT_SET1 = 0x16;
const uint8_t DA217_INT_SET1_INT_SOURCE_OVERSAMPLING = 0b00 << 6;
const uint8_t DA217_INT_SET1_INT_SOURCE_UNFILTERED = 0b01 << 6;
const uint8_t DA217_INT_SET1_INT_SOURCE_FILTERED = 0b10 << 6;
const uint8_t DA217_INT_SET1_ENABLE_SINGLE_TAP = 1 << 5;
const uint8_t DA217_INT_SET1_ENABLE_DOUBLE_TAP = 1 << 4;

const uint8_t DA217_REGISTER_INT_MAP1 = 0x19;
const uint8_t DA217_INT_MAP1_SM = 1 << 7;
const uint8_t DA217_INT_MAP1_ORIENT = 1 << 6;
const uint8_t DA217_INT_MAP1_SINGLE_TAP = 1 << 5;
const uint8_t DA217_INT_MAP1_DOUBLE_TAP = 1 << 4;
const uint8_t DA217_INT_MAP1_TILT = 1 << 3;
const uint8_t DA217_INT_MAP1_ACTIVE = 1 << 2;
const uint8_t DA217_INT_MAP1_STEP = 1 << 1;
const uint8_t DA217_INT_MAP1_FREEFALL = 1;

const uint8_t DA217_REGISTER_INT_LATCH = 0x21;
const uint8_t DA217_INT_LATCH_INT1_LATCH_100MS = 0b1110;
const uint8_t DA217_INT_LATCH_INT2_LATCH_100MS = 0b1110 << 4;

const uint8_t DA217_REGISTER_TAP_DUR = 0x2A;
const uint8_t DA217_TAP_DUR_QUIET_30MS = 0b0 << 7;
const uint8_t DA217_TAP_DUR_QUIET_20MS = 0b1 << 7;
const uint8_t DA217_TAP_DUR_SHOCK_50MS = 0b0 << 6;
const uint8_t DA217_TAP_DUR_SHOCK_70MS = 0b1 << 6;
const uint8_t DA217_TAP_DUR_50MS = 0b000;
const uint8_t DA217_TAP_DUR_100MS = 0b001;
const uint8_t DA217_TAP_DUR_150MS = 0b010;
const uint8_t DA217_TAP_DUR_200MS = 0b011;
const uint8_t DA217_TAP_DUR_250MS = 0b100;
const uint8_t DA217_TAP_DUR_370MS = 0b101;
const uint8_t DA217_TAP_DUR_500MS = 0b110;
const uint8_t DA217_TAP_DUR_700MS = 0b111;

const uint8_t DA217_REGISTER_TAP_THS = 0x2B;
const uint8_t DA217_TAP_THS_TAP_TH_MASK = 0b11111;

const float GRAVITY_EARTH = 9.80665f;

void DA217Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up DA217...");
  uint8_t chip_id;
  if (!this->read_byte(DA217_REGISTER_CHIP_ID, &chip_id) || (chip_id != 0x13)) {
    this->mark_failed();
    return;
  }

  ESP_LOGV(TAG, "  Setting up bus config...");
  if (!this->write_byte(DA217_REGISTER_SPI_CONFIG, DA217_SPI_CONFIG_SDO_ACTIVE | DA217_SPI_CONFIG_SOFT_RESET)) {
    this->mark_failed();
    return;
  }

  ESP_LOGV(TAG, "  Setting up resolution...");
  set_register_(DA217_REGISTER_RESOLUTION_RANGE, resolution_range_);

  ESP_LOGV(TAG, "  Setting up axes and frequency...");
  set_register_(DA217_REGISTER_ODR_AXIS, odr_axis_);

  ESP_LOGV(TAG, "  Setting up interrupt latching...");
  if (!this->write_byte(DA217_REGISTER_INT_LATCH,
                        DA217_INT_LATCH_INT1_LATCH_100MS | DA217_INT_LATCH_INT2_LATCH_100MS)) {
    this->mark_failed();
    return;
  }

  ESP_LOGV(TAG, "  Setting up tap duration...");
  set_register_(DA217_REGISTER_TAP_DUR, tap_dur_);

  ESP_LOGV(TAG, "  Setting up tap threshold...");
  set_register_(DA217_REGISTER_TAP_THS, tap_ths_);

  ESP_LOGV(TAG, "  Setting up interrupt mapping...");
  set_register_(DA217_REGISTER_INT_MAP1, int_map1_);

  ESP_LOGV(TAG, "  Setting up interrupt settings...");
  set_register_(DA217_REGISTER_INT_SET1, int_set1_);

  ESP_LOGV(TAG, "  Setting power mode...");
  if (!this->write_byte(DA217_REGISTER_MODE_BW, DA217_MODE_BW_NORMAL_MODE)) {
    this->mark_failed();
    return;
  }
}

void DA217Component::dump_config() {
  ESP_LOGCONFIG(TAG, "DA217:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with DA217 failed!");
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Acceleration X", this->accel_x_sensor_);
  LOG_SENSOR("  ", "Acceleration Y", this->accel_y_sensor_);
  LOG_SENSOR("  ", "Acceleration Z", this->accel_z_sensor_);

  ESP_LOGCONFIG(TAG, "  resolution_range (0x%x) = 0x%x", DA217_REGISTER_RESOLUTION_RANGE, resolution_range_);
  ESP_LOGCONFIG(TAG, "  odr_axis (0x%x) =0x%x", DA217_REGISTER_ODR_AXIS, odr_axis_);
  ESP_LOGCONFIG(TAG, "  int_set1 (0x%x) =0x%x", DA217_REGISTER_INT_SET1, int_set1_);
  ESP_LOGCONFIG(TAG, "  int_map1 (0x%x)=0x%x", DA217_REGISTER_INT_MAP1, int_map1_);
  ESP_LOGCONFIG(TAG, "  tap_dur (0x%x) =0x%x", DA217_REGISTER_TAP_DUR, tap_dur_);
  ESP_LOGCONFIG(TAG, "  tap_ths (0x%x) =0x%x", DA217_REGISTER_TAP_THS, tap_ths_);
}

void DA217Component::update() {
  ESP_LOGV(TAG, "    Updating DA217...");

  uint8_t status = 0;
  this->read_byte(0x11, &status);
  ESP_LOGD(TAG, "Register 0x11 is %d", status);

  int16_t raw_accel_x;
  int16_t raw_accel_y;
  int16_t raw_accel_z;

  uint8_t *raw_accel_x_lsb_ptr = reinterpret_cast<uint8_t *>(&raw_accel_x);
  uint8_t *raw_accel_x_msb_ptr = raw_accel_x_lsb_ptr + 1;
  uint8_t *raw_accel_y_lsb_ptr = reinterpret_cast<uint8_t *>(&raw_accel_y);
  uint8_t *raw_accel_y_msb_ptr = raw_accel_y_lsb_ptr + 1;
  uint8_t *raw_accel_z_lsb_ptr = reinterpret_cast<uint8_t *>(&raw_accel_z);
  uint8_t *raw_accel_z_msb_ptr = raw_accel_z_lsb_ptr + 1;

  if (!this->read_byte(DA217_REGISTER_ACC_X_LSB, raw_accel_x_lsb_ptr) ||
      !this->read_byte(DA217_REGISTER_ACC_X_MSB, raw_accel_x_msb_ptr) ||
      !this->read_byte(DA217_REGISTER_ACC_Y_LSB, raw_accel_y_lsb_ptr) ||
      !this->read_byte(DA217_REGISTER_ACC_Y_MSB, raw_accel_y_msb_ptr) ||
      !this->read_byte(DA217_REGISTER_ACC_Z_LSB, raw_accel_z_lsb_ptr) ||
      !this->read_byte(DA217_REGISTER_ACC_Z_MSB, raw_accel_z_msb_ptr)) {
    this->status_set_warning();
    return;
  }

  // The sensor has been configured to use a scale of ±4g, so a span of 8g.
  // It also "left-justifies" the data it outputs, so no matter its actual
  // resolution, the reported values will use the full 16 bit span.
  float g_per_bit = 8.0f / (1 << 16);
  float accel_x = raw_accel_x * g_per_bit * GRAVITY_EARTH;
  float accel_y = raw_accel_y * g_per_bit * GRAVITY_EARTH;
  float accel_z = raw_accel_z * g_per_bit * GRAVITY_EARTH;

  ESP_LOGD(TAG, "Got accel={x=%.3f m/s², y=%.3f m/s², z=%.3f m/s²}, ", accel_x, accel_y, accel_z);

  if (this->accel_x_sensor_ != nullptr)
    this->accel_x_sensor_->publish_state(accel_x);
  if (this->accel_y_sensor_ != nullptr)
    this->accel_y_sensor_->publish_state(accel_y);
  if (this->accel_z_sensor_ != nullptr)
    this->accel_z_sensor_->publish_state(accel_z);

  this->status_clear_warning();
}

float DA217Component::get_setup_priority() const { return setup_priority::DATA; }

void DA217Component::set_resolution_range(bool hp_en, bool wdt_en, WatchdogTime wdt_time, Resolution resolution,
                                          FullScale fs) {
  resolution_range_ = hp_en << 7 | wdt_en << 6 | wdt_time << 5 | resolution << 2 | fs;
}

void DA217Component::set_odr_axis(bool x_axis_disable, bool y_axis_disable, bool z_axis_disable, OutputDataRate odr) {
  odr_axis_ = x_axis_disable << 7 | y_axis_disable << 6 | z_axis_disable << 5 | odr;
}

void DA217Component::set_int_set1(InterruptSource int_source, bool s_tap_int_en, bool d_tap_int_en, bool orient_int_en,
                                  bool active_int_en_z, bool active_int_en_y, bool active_int_en_x) {
  int_set1_ = int_source << 6 | s_tap_int_en << 5 | d_tap_int_en << 4 | orient_int_en << 3 | active_int_en_z << 2 |
              active_int_en_y << 1 | active_int_en_x;
}

void DA217Component::set_int_map1(bool int1_sm, bool int1_orient, bool int1_s_tap, bool int1_d_tap, bool int1_tilt,
                                  bool int1_active, bool int1_step, bool int1_freefall) {
  int_map1_ = int1_sm << 7 | int1_orient << 6 | int1_s_tap << 5 | int1_d_tap << 4 | int1_tilt << 3 | int1_active << 2 |
              int1_step << 1 | int1_freefall;
}

void DA217Component::set_tap_dur(TapQuietDuration tap_quiet, TapShockDuration tap_shock, DoubleTapDuration tap_dur) {
  tap_dur_ = tap_quiet << 7 | tap_shock << 6 | tap_dur;
}

void DA217Component::set_tap_ths(StableTiltTime tilt_time, uint8_t tap_th) {
  // The datasheet uses a weird K constant to explain a simple concept. In fact
  // tap_th uses 5 bits to describe the sensor's whole acceleration range. For
  // instance, in the ±4g range, a value of 0x1F for tap_th will mean a tap
  // threshold of 4g, and a value of 0 would mean 0g.
  tap_ths_ = tilt_time << 5 | (tap_th & DA217_TAP_THS_TAP_TH_MASK);
}

void DA217Component::set_register_(uint8_t a_register, uint8_t data) {
  ESP_LOGD(TAG, "Set register %x to %x", a_register, data);
  if (!this->write_byte(a_register, data)) {
    ESP_LOGE(TAG, "Error setting register %x to %x", a_register, data);
    this->mark_failed();
    return;
  }
}

}  // namespace da217
}  // namespace esphome
