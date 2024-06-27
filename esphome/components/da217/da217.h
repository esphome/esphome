#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

// Support for MiraMEMS DA2xx accelerometers
// https://resource.heltec.cn/download/sensor/DA217/DA217-%E8%BF%90%E5%8A%A8.pdf
// This code is both Public Domain and MIT licensed

namespace esphome {
namespace da217 {

enum WatchdogTime { TIME1MS = 0, TIME50MS = 1 };

enum Resolution { RESOLUTION14BITS = 0b00, RESOLUTION12BITS = 0b01, RESOLUTION10BITS = 0b10, RESOLUTION8BITS = 0b11 };

enum FullScale { PLUS_MINUS2G = 0b00, PLUS_MINUS4G = 0b01, PLUS_MINUS8G = 0b10, PLUS_MINUS16G = 0b11 };

enum OutputDataRate {
  RATE1_HZ = 0b0000,
  RATE1P95_HZ = 0b0001,
  RATE3P9_HZ = 0b0010,
  RATE7P81_HZ = 0b0011,
  RATE15P63_HZ = 0b0100,
  RATE31P25_HZ = 0b0101,
  RATE62P5_HZ = 0b0110,
  RATE125_HZ = 0b0111,
  RATE250_HZ = 0b1000,
  RATE500_HZ = 0b1001,
  UNCONFIGURED = 0b1111
};

enum InterruptSource { OVERSAMPLING = 0b00, UNFILTERED = 0b01, FILTERED = 0b10 };

enum TapQuietDuration {
  QUIET30MS = 0,
  QUIET20MS = 1,
};

enum TapShockDuration { SHOCK50MS = 0, SHOCK70MS = 1 };

enum DoubleTapDuration {
  DOUBLE_TAP50MS = 0b000,
  DOUBLE_TAP100MS = 0b001,
  DOUBLE_TAP150MS = 0b010,
  DOUBLE_TAP200MS = 0b011,
  DOUBLE_TAP250MS = 0b100,
  DOUBLE_TAP375MS = 0b101,
  DOUBLE_TAP500MS = 0b110,
  DOUBLE_TAP700MS = 0b111,
};

enum StableTiltTime { ODR32 = 0b00, ODR96 = 0b01, ODR160 = 0b10, ODR224 = 0b11 };

class DA217Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override;
  void set_accel_x_sensor(sensor::Sensor *accel_x_sensor) { accel_x_sensor_ = accel_x_sensor; }
  void set_accel_y_sensor(sensor::Sensor *accel_y_sensor) { accel_y_sensor_ = accel_y_sensor; }
  void set_accel_z_sensor(sensor::Sensor *accel_z_sensor) { accel_z_sensor_ = accel_z_sensor; }
  void set_resolution_range(bool hp_en, bool wdt_en, WatchdogTime wdt_time, Resolution resolution, FullScale fs);
  void set_odr_axis(bool x_axis_disable, bool y_axis_disable, bool z_axis_disable, OutputDataRate odr);
  void set_int_set1(InterruptSource int_source, bool s_tap_int_en, bool d_tap_int_en, bool orient_int_en,
                    bool active_int_en_z, bool active_int_en_y, bool active_int_en_x);
  void set_int_map1(bool int1_sm, bool int1_orient, bool int1_s_tap, bool int1_d_tap, bool int1_tilt, bool int1_active,
                    bool int1_step, bool int1_freefall);
  void set_tap_dur(TapQuietDuration tap_quiet, TapShockDuration tap_shock, DoubleTapDuration tap_dur);
  void set_tap_ths(StableTiltTime tilt_time, uint8_t tap_th);

 protected:
  sensor::Sensor *accel_x_sensor_{nullptr};
  sensor::Sensor *accel_y_sensor_{nullptr};
  sensor::Sensor *accel_z_sensor_{nullptr};
  void set_register_(uint8_t a_register, uint8_t data);
  uint8_t resolution_range_;
  uint8_t odr_axis_;
  uint8_t int_set1_;
  uint8_t int_map1_;
  uint8_t tap_dur_;
  uint8_t tap_ths_;
};

}  // namespace da217
}  // namespace esphome
