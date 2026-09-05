#pragma once

#include "esphome/core/component.h"
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

namespace esphome::lis2dw12_base {

// LIS2DW12 Register Map
enum class RegisterMap : uint8_t {
  OUT_T_L = 0x0D,
  OUT_T_H = 0x0E,
  WHO_AM_I = 0x0F,
  CTRL1 = 0x20,
  CTRL2 = 0x21,
  CTRL3 = 0x22,
  CTRL4_INT1 = 0x23,
  CTRL5_INT2 = 0x24,
  CTRL6 = 0x25,
  OUT_T = 0x26,
  STATUS = 0x27,
  OUT_X_L = 0x28,
  OUT_X_H = 0x29,
  OUT_Y_L = 0x2A,
  OUT_Y_H = 0x2B,
  OUT_Z_L = 0x2C,
  OUT_Z_H = 0x2D,
  FIFO_CTRL = 0x2E,
  FIFO_SAMPLES = 0x2F,
  TAP_THS_X = 0x30,
  TAP_THS_Y = 0x31,
  TAP_THS_Z = 0x32,
  INT_DUR = 0x33,
  WAKE_UP_THS = 0x34,
  WAKE_UP_DUR = 0x35,
  FREE_FALL = 0x36,
  STATUS_DUP = 0x37,
  WAKE_UP_SRC = 0x38,
  TAP_SRC = 0x39,
  SIXD_SRC = 0x3A,
  ALL_INT_SRC = 0x3B,
  X_OFS_USR = 0x3C,
  Y_OFS_USR = 0x3D,
  Z_OFS_USR = 0x3E,
  CTRL7 = 0x3F,
};

enum class Range : uint8_t {
  RANGE_2G = 0b00,
  RANGE_4G = 0b01,
  RANGE_8G = 0b10,
  RANGE_16G = 0b11,
};

enum class PowerMode : uint8_t {
  HIGH_PERF = 0,
  LOW_POWER_1 = 1,
  LOW_POWER_2 = 2,
  LOW_POWER_3 = 3,
  LOW_POWER_4 = 4,
};

enum class DataRate : uint8_t {
  RATE_POWER_DOWN = 0b0000,
  RATE_1_6HZ = 0b0001,
  RATE_12_5HZ = 0b0010,
  RATE_25HZ = 0b0011,
  RATE_50HZ = 0b0100,
  RATE_100HZ = 0b0101,
  RATE_200HZ = 0b0110,
  RATE_400HZ = 0b0111,
  RATE_800HZ = 0b1000,
  RATE_1600HZ = 0b1001,
};

enum class FilterBandwidth : uint8_t {
  BW_ODR_DIV_2 = 0b00,
  BW_ODR_DIV_4 = 0b01,
  BW_ODR_DIV_10 = 0b10,
  BW_ODR_DIV_20 = 0b11,
};

// ALL_INT_SRC register (0x3B)
union RegAllIntSrc {
  struct {
    bool ff_ia : 1;
    bool wu_ia : 1;
    bool single_tap : 1;
    bool double_tap : 1;
    bool d6d_ia : 1;
    bool sleep_change_ia : 1;
    uint8_t reserved : 2;
  } __attribute__((packed));
  uint8_t raw{0};
};

// WAKE_UP_SRC register (0x38)
union RegWakeUpSrc {
  struct {
    bool z_wu : 1;
    bool y_wu : 1;
    bool x_wu : 1;
    bool wu_ia : 1;
    bool sleep_state_ia : 1;
    bool ff_ia : 1;
    uint8_t reserved : 2;
  } __attribute__((packed));
  uint8_t raw{0};
};

// TAP_SRC register (0x39)
union RegTapSrc {
  struct {
    bool z_tap : 1;
    bool y_tap : 1;
    bool x_tap : 1;
    bool tap_sign : 1;
    bool double_tap : 1;
    bool single_tap : 1;
    bool tap_ia : 1;
    uint8_t reserved : 1;
  } __attribute__((packed));
  uint8_t raw{0};
};

// SIXD_SRC register (0x3A)
union RegSixdSrc {
  struct {
    bool xl : 1;
    bool xh : 1;
    bool yl : 1;
    bool yh : 1;
    bool zl : 1;
    bool zh : 1;
    bool d6d_ia : 1;
    uint8_t reserved : 1;
  } __attribute__((packed));
  uint8_t raw{0};
};

class LIS2DW12Component : public PollingComponent {
 public:
  void setup() override;
  void dump_config() override;
  void loop() override;
  void update() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_power_mode(PowerMode mode) { this->power_mode_ = mode; }
  void set_low_noise(bool low_noise) { this->low_noise_ = low_noise; }
  void set_output_data_rate(DataRate rate) { this->data_rate_ = rate; }
  void set_range(Range range) { this->range_ = range; }
  void set_filter_bandwidth(FilterBandwidth bw) { this->filter_bandwidth_ = bw; }
  void set_offset(float offset_x, float offset_y, float offset_z);
  void set_transform(bool mirror_x, bool mirror_y, bool mirror_z, bool swap_xy);

  virtual bool read_byte(uint8_t a_register, uint8_t *data) = 0;
  virtual bool write_byte(uint8_t a_register, uint8_t data) = 0;
  virtual bool read_bytes(uint8_t a_register, uint8_t *data, size_t len) = 0;

#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(tap)
  SUB_BINARY_SENSOR(double_tap)
  SUB_BINARY_SENSOR(freefall)
  SUB_BINARY_SENSOR(active)
#endif

#ifdef USE_SENSOR
  SUB_SENSOR(acceleration_x)
  SUB_SENSOR(acceleration_y)
  SUB_SENSOR(acceleration_z)
#endif

#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(orientation)
#endif

  Trigger<> *get_tap_trigger() { return &this->tap_trigger_; }
  Trigger<> *get_double_tap_trigger() { return &this->double_tap_trigger_; }
  Trigger<> *get_freefall_trigger() { return &this->freefall_trigger_; }
  Trigger<> *get_active_trigger() { return &this->active_trigger_; }
  Trigger<> *get_orientation_trigger() { return &this->orientation_trigger_; }

 protected:
  PowerMode power_mode_{PowerMode::HIGH_PERF};
  bool low_noise_{false};
  DataRate data_rate_{DataRate::RATE_100HZ};
  Range range_{Range::RANGE_2G};
  FilterBandwidth filter_bandwidth_{FilterBandwidth::BW_ODR_DIV_2};
  float offset_x_{0}, offset_y_{0}, offset_z_{0};
  bool mirror_x_{false}, mirror_y_{false}, mirror_z_{false}, swap_xy_{false};

  float sensitivity_{0.244f};  // mg per LSB (14-bit, 2G default)

  struct {
    float x, y, z;
  } data_{};

  struct {
    RegAllIntSrc all_int;
    RegSixdSrc sixd;
    RegSixdSrc sixd_old;

    uint32_t last_tap_ms{0};
    uint32_t last_double_tap_ms{0};
    uint32_t last_freefall_ms{0};
    uint32_t last_active_ms{0};

    bool never_published{true};
  } status_{};

  bool configure_registers_();
  bool read_acceleration_data_();
  bool read_interrupt_status_();
  void process_events_();
  const char *get_orientation_string_();

  Trigger<> tap_trigger_;
  Trigger<> double_tap_trigger_;
  Trigger<> freefall_trigger_;
  Trigger<> active_trigger_;
  Trigger<> orientation_trigger_;
};

}  // namespace esphome::lis2dw12_base
