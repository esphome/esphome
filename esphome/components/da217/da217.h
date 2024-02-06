#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

// Support for MiraMEMS DA2xx accelerometers
// https://resource.heltec.cn/download/sensor/DA217/DA217-%E8%BF%90%E5%8A%A8.pdf
// This code is both Public Domain and MIT licensed

namespace esphome {
namespace da217 {

enum WatchdogTime { Time1ms = 0, Time50ms = 1 };

enum Resolution { Resolution14bits = 0b00, Resolution12bits = 0b01, Resolution10bits = 0b10, Resolution8bits = 0b11 };

enum FullScale { PlusMinus2g = 0b00, PlusMinus4g = 0b01, PlusMinus8g = 0b10, PlusMinus16g = 0b11 };

enum OutputDataRate {
  Rate1Hz = 0b0000,
  Rate1p95Hz = 0b0001,
  Rate3p9Hz = 0b0010,
  Rate7p81Hz = 0b0011,
  Rate15p63Hz = 0b0100,
  Rate31p25Hz = 0b0101,
  Rate62p5Hz = 0b0110,
  Rate125Hz = 0b0111,
  Rate250Hz = 0b1000,
  Rate500Hz = 0b1001,
  Unconfigured = 0b1111
};

enum InterruptSource { Oversampling = 0b00, Unfiltered = 0b01, Filtered = 0b10 };

enum TapQuietDuration {
  Quiet30ms = 0,
  Quiet20ms = 1,
};

enum TapShockDuration { Shock50ms = 0, Shock70ms = 1 };

enum DoubleTapDuration {
  DoubleTap50ms = 0b000,
  DoubleTap100ms = 0b001,
  DoubleTap150ms = 0b010,
  DoubleTap200ms = 0b011,
  DoubleTap250ms = 0b100,
  DoubleTap375ms = 0b101,
  DoubleTap500ms = 0b110,
  DoubleTap700ms = 0b111,
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
