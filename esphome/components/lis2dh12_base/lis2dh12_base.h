#pragma once

#include <utility>

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

namespace esphome::lis2dh12_base {

// LIS2DH12 Register Map
enum class RegisterMap : uint8_t {
  TEMP_CFG_REG = 0x1F,
  CTRL_REG1 = 0x20,
  CTRL_REG2 = 0x21,
  CTRL_REG3 = 0x22,
  CTRL_REG4 = 0x23,
  CTRL_REG5 = 0x24,
  CTRL_REG6 = 0x25,
  REFERENCE = 0x26,
  STATUS_REG = 0x27,
  OUT_X_L = 0x28,
  OUT_X_H = 0x29,
  OUT_Y_L = 0x2A,
  OUT_Y_H = 0x2B,
  OUT_Z_L = 0x2C,
  OUT_Z_H = 0x2D,
  FIFO_CTRL_REG = 0x2E,
  FIFO_SRC_REG = 0x2F,
  INT1_CFG = 0x30,
  INT1_SRC = 0x31,
  INT1_THS = 0x32,
  INT1_DURATION = 0x33,
  INT2_CFG = 0x34,
  INT2_SRC = 0x35,
  INT2_THS = 0x36,
  INT2_DURATION = 0x37,
  CLICK_CFG = 0x38,
  CLICK_SRC = 0x39,
  CLICK_THS = 0x3A,
  TIME_LIMIT = 0x3B,
  TIME_LATENCY = 0x3C,
  TIME_WINDOW = 0x3D,
  ACT_THS = 0x3E,
  ACT_DUR = 0x3F,
  WHO_AM_I = 0x0F,
};

enum class Range : uint8_t {
  RANGE_2G = 0b00,
  RANGE_4G = 0b01,
  RANGE_8G = 0b10,
  RANGE_16G = 0b11,
};

enum class Resolution : uint8_t {
  HIGH_RESOLUTION = 0,  // 12-bit
  NORMAL = 1,           // 10-bit
  LOW_POWER = 2,        // 8-bit
};

enum class DataRate : uint8_t {
  RATE_POWER_DOWN = 0b0000,
  RATE_1HZ = 0b0001,
  RATE_10HZ = 0b0010,
  RATE_25HZ = 0b0011,
  RATE_50HZ = 0b0100,
  RATE_100HZ = 0b0101,
  RATE_200HZ = 0b0110,
  RATE_400HZ = 0b0111,
  RATE_1620HZ_LP = 0b1000,
  RATE_5376HZ_LP = 0b1001,
};

// CLICK_SRC register (0x39)
union RegClickSrc {
  struct {
    bool x : 1;
    bool y : 1;
    bool z : 1;
    bool sign : 1;
    bool sclick : 1;
    bool dclick : 1;
    bool ia : 1;
    uint8_t reserved : 1;
  } __attribute__((packed));
  uint8_t raw{0};
};

// INT1_SRC register (0x31) / INT2_SRC register (0x35) — same bit layout
union RegIntSrc {
  struct {
    bool xl : 1;
    bool xh : 1;
    bool yl : 1;
    bool yh : 1;
    bool zl : 1;
    bool zh : 1;
    bool ia : 1;
    uint8_t reserved : 1;
  } __attribute__((packed));
  uint8_t raw{0};
};

class LIS2DH12Component : public PollingComponent {
 public:
  void setup() override;
  void dump_config() override;
  void loop() override;
  void update() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_resolution(Resolution res) { this->resolution_ = res; }
  void set_output_data_rate(DataRate rate) { this->data_rate_ = rate; }
  void set_range(Range range) { this->range_ = range; }
  void set_offset(float offset_x, float offset_y, float offset_z);
  void set_transform(bool mirror_x, bool mirror_y, bool mirror_z, bool swap_xy);

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

  // Bus abstraction - implemented by I2C/SPI variants
  virtual bool read_byte(uint8_t a_register, uint8_t *data) = 0;
  virtual bool write_byte(uint8_t a_register, uint8_t data) = 0;
  virtual bool read_bytes(uint8_t a_register, uint8_t *data, size_t len) = 0;

 protected:
  Resolution resolution_{Resolution::HIGH_RESOLUTION};
  DataRate data_rate_{DataRate::RATE_100HZ};
  Range range_{Range::RANGE_2G};
  float offset_x_{0}, offset_y_{0}, offset_z_{0};
  bool mirror_x_{false}, mirror_y_{false}, mirror_z_{false}, swap_xy_{false};

  float sensitivity_{1.0f};  // mg per LSB (HR, 2G default)

  struct {
    float x, y, z;
  } data_{};

  struct {
    RegClickSrc click;
    RegIntSrc int1;  // 6D orientation (INT1_SRC)
    RegIntSrc int1_old;
    RegIntSrc int2;  // Freefall/activity (INT2_SRC)

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

}  // namespace esphome::lis2dh12_base
