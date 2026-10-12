#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

namespace esphome::lis2dw12 {

// LIS2DW12 Register Map
enum class RegisterMap : uint8_t {
  WHO_AM_I = 0x0F,
  CTRL1 = 0x20,
  CTRL2 = 0x21,
  CTRL3 = 0x22,
  CTRL4_INT1 = 0x23,
  CTRL6 = 0x25,
  OUT_X_L = 0x28,
  TAP_THS_X = 0x30,
  TAP_THS_Y = 0x31,
  TAP_THS_Z = 0x32,
  INT_DUR = 0x33,
  WAKE_UP_THS = 0x34,
  WAKE_UP_DUR = 0x35,
  FREE_FALL = 0x36,
  SIXD_SRC = 0x3A,
  ALL_INT_SRC = 0x3B,
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

// Debounced events, also the bit index into events_on_
enum Event : uint8_t {
  EVENT_TAP = 0,
  EVENT_DOUBLE_TAP,
  EVENT_FREEFALL,
  EVENT_ACTIVE,
  EVENT_COUNT,
};

class LIS2DW12Component : public PollingComponent {
 public:
  void setup() override;
  void dump_config() override;
  void loop() override;
  void update() override;

  void set_power_mode(PowerMode mode) { this->power_mode_ = mode; }
  void set_low_noise(bool low_noise) { this->low_noise_ = low_noise; }
  void set_output_data_rate(DataRate rate) { this->data_rate_ = rate; }
  void set_range(Range range) { this->range_ = range; }
  void set_filter_bandwidth(FilterBandwidth bw) { this->filter_bandwidth_ = bw; }
  void set_offset(float offset_x, float offset_y, float offset_z);
  void set_transform(bool mirror_x, bool mirror_y, bool mirror_z, bool swap_xy);

  bool read_byte(uint8_t a_register, uint8_t *data) { return this->read_bytes(a_register, data, 1); }
  virtual bool write_byte(uint8_t a_register, uint8_t data) = 0;
  virtual bool read_bytes(uint8_t a_register, uint8_t *data, size_t len) = 0;

#ifdef USE_BINARY_SENSOR
  void set_tap_binary_sensor(binary_sensor::BinarySensor *s) { this->event_binary_sensors_[EVENT_TAP] = s; }
  void set_double_tap_binary_sensor(binary_sensor::BinarySensor *s) {
    this->event_binary_sensors_[EVENT_DOUBLE_TAP] = s;
  }
  void set_freefall_binary_sensor(binary_sensor::BinarySensor *s) { this->event_binary_sensors_[EVENT_FREEFALL] = s; }
  void set_active_binary_sensor(binary_sensor::BinarySensor *s) { this->event_binary_sensors_[EVENT_ACTIVE] = s; }
#endif

#ifdef USE_SENSOR
  SUB_SENSOR(acceleration_x)
  SUB_SENSOR(acceleration_y)
  SUB_SENSOR(acceleration_z)
#endif

#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(orientation)
#endif

  template<typename F> void add_on_tap_callback(F &&callback) { this->tap_callback_.add(std::forward<F>(callback)); }
  template<typename F> void add_on_double_tap_callback(F &&callback) {
    this->double_tap_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_freefall_callback(F &&callback) {
    this->freefall_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_active_callback(F &&callback) {
    this->active_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_orientation_callback(F &&callback) {
    this->orientation_callback_.add(std::forward<F>(callback));
  }

 protected:
  bool write_reg_(RegisterMap reg, uint8_t value) { return this->write_byte(static_cast<uint8_t>(reg), value); }
  bool configure_registers_();
  bool read_acceleration_data_(float &x, float &y, float &z);
  void handle_event_(Event event, bool active, uint32_t now, LazyCallbackManager<void()> &callback);
#ifdef USE_TEXT_SENSOR
  void publish_orientation_(RegSixdSrc sixd);
#endif

  float offset_x_{0}, offset_y_{0}, offset_z_{0};
  float scale_{0};  // m/s² per LSB after shift_
  uint32_t last_event_ms_[EVENT_COUNT]{};
  uint32_t last_poll_ms_{0};
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *event_binary_sensors_[EVENT_COUNT]{};
#endif
  LazyCallbackManager<void()> tap_callback_;
  LazyCallbackManager<void()> double_tap_callback_;
  LazyCallbackManager<void()> freefall_callback_;
  LazyCallbackManager<void()> active_callback_;
  LazyCallbackManager<void()> orientation_callback_;

  PowerMode power_mode_{PowerMode::HIGH_PERF};
  DataRate data_rate_{DataRate::RATE_100HZ};
  Range range_{Range::RANGE_2G};
  FilterBandwidth filter_bandwidth_{FilterBandwidth::BW_ODR_DIV_2};
  bool low_noise_{false};
  bool mirror_x_{false}, mirror_y_{false}, mirror_z_{false}, swap_xy_{false};
  uint8_t shift_{2};      // data is left-justified: 14-bit >> 2, 12-bit (LP1) >> 4
  uint8_t events_on_{0};  // bit per Event: binary sensor currently published on
#ifdef USE_TEXT_SENSOR
  uint8_t sixd_last_{0xFF};  // position bits only; 0xFF cannot occur, so the first read publishes
#endif
};

}  // namespace esphome::lis2dw12
