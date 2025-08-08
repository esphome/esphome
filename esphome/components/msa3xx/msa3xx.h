#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/automation.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

namespace esphome {
namespace msa3xx {

// Combined register map of MSA301 and MSA311
// Differences
//  What             |  MSA301 | MSA11  |
//  - Resolution     |  14-bit | 12-bit |
//

// I2c address
enum class Model : uint8_t {
  MSA301 = 0x26,
  MSA311 = 0x62,
};

// Combined MSA301 and MSA311 register map
enum class RegisterMap : uint8_t {
  SOFT_RESET = 0x00,
  PART_ID = 0x01,
  ACC_X_LSB = 0x02,
  ACC_X_MSB = 0x03,
  ACC_Y_LSB = 0x04,
  ACC_Y_MSB = 0x05,
  ACC_Z_LSB = 0x06,
  ACC_Z_MSB = 0x07,
  MOTION_INTERRUPT = 0x09,
  DATA_INTERRUPT = 0x0A,
  TAP_ACTIVE_STATUS = 0x0B,
  ORIENTATION_STATUS = 0x0C,
  RESOLUTION_RANGE_CONFIG = 0x0D,
  RANGE_RESOLUTION = 0x0F,
  ODR = 0x10,
  POWER_MODE_BANDWIDTH = 0x11,
  SWAP_POLARITY = 0x12,
  INT_SET_0 = 0x16,
  INT_SET_1 = 0x17,
  INT_MAP_0 = 0x19,
  INT_MAP_1 = 0x1A,
  INT_CONFIG = 0x20,
  INT_LATCH = 0x21,
  FREEFALL_DURATION = 0x22,
  FREEFALL_THRESHOLD = 0x23,
  FREEFALL_HYSTERESIS = 0x24,
  ACTIVE_DURATION = 0x27,
  ACTIVE_THRESHOLD = 0x28,
  TAP_DURATION = 0x2A,
  TAP_THRESHOLD = 0x2B,
  ORIENTATION_CONFIG = 0x2C,
  Z_BLOCK = 0x2D,
  OFFSET_COMP_X = 0x38,
  OFFSET_COMP_Y = 0x39,
  OFFSET_COMP_Z = 0x3A,
};

enum class Range : uint8_t {
  RANGE_2G = 0b00,
  RANGE_4G = 0b01,
  RANGE_8G = 0b10,
  RANGE_16G = 0b11,
};

enum class Resolution : uint8_t {
  RES_14BIT = 0b00,
  RES_12BIT = 0b01,
  RES_10BIT = 0b10,
  RES_8BIT = 0b11,
};

enum class PowerMode : uint8_t {
  NORMAL = 0b00,
  LOW_POWER = 0b01,
  SUSPEND = 0b11,
};

enum class Bandwidth : uint8_t {
  BW_1_95HZ = 0b0000,
  BW_3_9HZ = 0b0011,
  BW_7_81HZ = 0b0100,
  BW_15_63HZ = 0b0101,
  BW_31_25HZ = 0b0110,
  BW_62_5HZ = 0b0111,
  BW_125HZ = 0b1000,
  BW_250HZ = 0b1001,
  BW_500HZ = 0b1010,
};

enum class DataRate : uint8_t {
  ODR_1HZ = 0b0000,     // not available in normal mode
  ODR_1_95HZ = 0b0001,  // not available in normal mode
  ODR_3_9HZ = 0b0010,
  ODR_7_81HZ = 0b0011,
  ODR_15_63HZ = 0b0100,
  ODR_31_25HZ = 0b0101,
  ODR_62_5HZ = 0b0110,
  ODR_125HZ = 0b0111,
  ODR_250HZ = 0b1000,
  ODR_500HZ = 0b1001,   // not available in low power mode
  ODR_1000HZ = 0b1010,  // not available in low power mode
};

enum class OrientationXY : uint8_t {
  PORTRAIT_UPRIGHT = 0b00,
  PORTRAIT_UPSIDE_DOWN = 0b01,
  LANDSCAPE_LEFT = 0b10,
  LANDSCAPE_RIGHT = 0b11,
};

union Orientation {
  struct {
    OrientationXY xy : 2;
    bool z : 1;
    uint8_t reserved : 5;
  } __attribute__((packed));
  uint8_t raw;
};

// 0x09
union RegMotionInterrupt {
  struct {
    bool freefall_interrupt : 1;
    bool reserved_1 : 1;
    bool active_interrupt : 1;
    bool reserved_3 : 1;
    bool double_tap_interrupt : 1;
    bool single_tap_interrupt : 1;
    bool orientation_interrupt : 1;
    bool reserved_7 : 1;
  } __attribute__((packed));
  uint8_t raw;
};

// 0x0C
union RegOrientationStatus {
  struct {
    uint8_t reserved_0_3 : 4;
    OrientationXY orient_xy : 2;
    bool orient_z : 1;
    uint8_t reserved_7 : 1;
  } __attribute__((packed));
  uint8_t raw{0x00};
};

// 0x0f
union RegRangeResolution {
  struct {
    Range range : 2;
    Resolution resolution : 2;
    uint8_t reserved_2 : 4;
  } __attribute__((packed));
  uint8_t raw{0x00};
};

// 0x10
union RegOutputDataRate {
  struct {
    DataRate odr : 4;
    uint8_t reserved_4 : 1;
    bool z_axis_disable : 1;
    bool y_axis_disable : 1;
    bool x_axis_disable : 1;
  } __attribute__((packed));
  uint8_t raw{0xde};
};

// 0x11
union RegPowerModeBandwidth {
  struct {
    uint8_t reserved_0 : 1;
    Bandwidth low_power_bandwidth : 4;
    uint8_t reserved_5 : 1;
    PowerMode power_mode : 2;
  } __attribute__((packed));
  uint8_t raw{0xde};
};

// 0x12
union RegSwapPolarity {
  struct {
    bool x_y_swap : 1;
    bool z_polarity : 1;
    bool y_polarity : 1;
    bool x_polarity : 1;
    uint8_t reserved : 4;
  } __attribute__((packed));
  uint8_t raw{0};
};

// 0x2a
union RegTapDuration {
  struct {
    uint8_t duration : 3;
    uint8_t reserved : 3;
    bool tap_shock : 1;
    bool tap_quiet : 1;
  } __attribute__((packed));
  uint8_t raw{0x04};
};

class MSA3xxComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;

  void loop() override;
  void update() override;

  float get_setup_priority() const override;

  void set_model(Model model) { this->model_ = model; }
  void set_offset(float offset_x, float offset_y, float offset_z);
  void set_range(Range range) { this->range_ = range; }
  void set_bandwidth(Bandwidth bandwidth) { this->bandwidth_ = bandwidth; }
  void set_resolution(Resolution resolution) { this->resolution_ = resolution; }
  void set_transform(bool mirror_x, bool mirror_y, bool mirror_z, bool swap_xy);

#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(tap)
  SUB_BINARY_SENSOR(double_tap)
  SUB_BINARY_SENSOR(active)
#endif

#ifdef USE_SENSOR
  SUB_SENSOR(acceleration_x)
  SUB_SENSOR(acceleration_y)
  SUB_SENSOR(acceleration_z)
#endif

#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(orientation_xy)
  SUB_TEXT_SENSOR(orientation_z)
#endif

  Trigger<> *get_tap_trigger() { return &this->tap_trigger_; }
  Trigger<> *get_double_tap_trigger() { return &this->double_tap_trigger_; }
  Trigger<> *get_orientation_trigger() { return &this->orientation_trigger_; }
  Trigger<> *get_freefall_trigger() { return &this->freefall_trigger_; }
  Trigger<> *get_active_trigger() { return &this->active_trigger_; }

 protected:
  Model model_{Model::MSA311};

  PowerMode power_mode_{PowerMode::NORMAL};
  DataRate data_rate_{DataRate::ODR_250HZ};
  Bandwidth bandwidth_{Bandwidth::BW_250HZ};
  Range range_{Range::RANGE_2G};
  Resolution resolution_{Resolution::RES_14BIT};
  float offset_x_, offset_y_, offset_z_;  // in m/s²
  RegSwapPolarity swap_;

  struct {
    int scale_factor_exp;
    uint8_t accel_data_width;
  } device_params_{};

  struct {
    int16_t lsb_x, lsb_y, lsb_z;
    float x, y, z;
  } data_{};

  struct {
    RegMotionInterrupt motion_int;
    RegOrientationStatus orientation;
    RegOrientationStatus orientation_old;

    uint32_t last_tap_ms{0};
    uint32_t last_double_tap_ms{0};
    uint32_t last_action_ms{0};

    bool never_published{true};
  } status_{};

  void setup_odr_(DataRate rate);
  void setup_power_mode_bandwidth_(PowerMode power_mode, Bandwidth bandwidth);
  void setup_range_resolution_(Range range, Resolution resolution);
  void setup_offset_(float offset_x, float offset_y, float offset_z);

  bool read_data_();
  bool read_motion_status_();

  int64_t twos_complement_(uint64_t value, uint8_t bits);

  //
  // Actons / Triggers
  //
  Trigger<> tap_trigger_;
  Trigger<> double_tap_trigger_;
  Trigger<> orientation_trigger_;
  Trigger<> freefall_trigger_;
  Trigger<> active_trigger_;

  void process_motions_(RegMotionInterrupt old);
};

}  // namespace msa3xx
}  // namespace esphome
