#pragma once

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

namespace esphome::opt4048 {

enum class OPT4048Range : uint8_t {
  OPT4048_RANGE_2K2 = 0,
  OPT4048_RANGE_4K5 = 1,
  OPT4048_RANGE_9K = 2,
  OPT4048_RANGE_18K = 3,
  OPT4048_RANGE_36K = 4,
  OPT4048_RANGE_72K = 5,
  OPT4048_RANGE_144K = 6,
  OPT4048_RANGE_AUTO = 12,
};

enum class OPT4048ConversionTime : uint8_t {
  OPT4048_CONVERSION_TIME_600US = 0,
  OPT4048_CONVERSION_TIME_1MS = 1,
  OPT4048_CONVERSION_TIME_1_8MS = 2,
  OPT4048_CONVERSION_TIME_3_4MS = 3,
  OPT4048_CONVERSION_TIME_6_5MS = 4,
  OPT4048_CONVERSION_TIME_12_7MS = 5,
  OPT4048_CONVERSION_TIME_25MS = 6,
  OPT4048_CONVERSION_TIME_50MS = 7,
  OPT4048_CONVERSION_TIME_100MS = 8,
  OPT4048_CONVERSION_TIME_200MS = 9,
  OPT4048_CONVERSION_TIME_400MS = 10,
  OPT4048_CONVERSION_TIME_800MS = 11,
};

enum class OPT4048Mode : uint8_t {
  OPT4048_MODE_POWERDOWN = 0,
  OPT4048_MODE_AUTO_ONESHOT = 1,
  OPT4048_MODE_ONESHOT = 2,
  OPT4048_MODE_CONTINUOUS = 3,
};

enum class OPT4048FaultCount : uint8_t {
  OPT4048_FAULT_COUNT_1 = 0,
  OPT4048_FAULT_COUNT_2 = 1,
  OPT4048_FAULT_COUNT_4 = 2,
  OPT4048_FAULT_COUNT_8 = 3,
};

enum class OPT4048IntConfig : uint8_t {
  OPT4048_INT_CONFIG_SMBUS_ALERT = 0,
  OPT4048_INT_CONFIG_DATA_READY_NEXT = 1,
  OPT4048_INT_CONFIG_DATA_READY_ALL = 3,
};

class OPT4048Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override;

  void set_range(OPT4048Range range) { this->range_ = range; }
  void set_conversion_time(OPT4048ConversionTime conversion_time) { this->conversion_time_ = conversion_time; }
  void set_mode(OPT4048Mode mode) { this->mode_ = mode; }
  void set_quick_wake(bool quick_wake) { this->quick_wake_ = quick_wake; }
  void set_interrupt_latch(bool latch) { this->interrupt_latch_ = latch; }
  void set_interrupt_polarity(bool active_high) { this->interrupt_polarity_high_ = active_high; }
  void set_fault_count(OPT4048FaultCount fault_count) { this->fault_count_ = fault_count; }
  void set_threshold_channel(uint8_t channel) { this->threshold_channel_ = channel; }
  void set_interrupt_direction(bool high_active) { this->interrupt_direction_high_ = high_active; }
  void set_interrupt_config(OPT4048IntConfig config) { this->interrupt_config_ = config; }
  void set_threshold_low(uint32_t value) {
    this->has_threshold_low_ = true;
    this->threshold_low_ = value;
  }
  void set_threshold_high(uint32_t value) {
    this->has_threshold_high_ = true;
    this->threshold_high_ = value;
  }
  void set_interrupt_pin(GPIOPin *pin) { this->interrupt_pin_ = pin; }

  void set_illuminance_sensor(sensor::Sensor *sensor) { this->illuminance_sensor_ = sensor; }
  void set_x_sensor(sensor::Sensor *sensor) { this->x_sensor_ = sensor; }
  void set_y_sensor(sensor::Sensor *sensor) { this->y_sensor_ = sensor; }
  void set_color_temperature_sensor(sensor::Sensor *sensor) { this->color_temperature_sensor_ = sensor; }
  void set_channel_x_sensor(sensor::Sensor *sensor) { this->channel_x_sensor_ = sensor; }
  void set_channel_y_sensor(sensor::Sensor *sensor) { this->channel_y_sensor_ = sensor; }
  void set_channel_z_sensor(sensor::Sensor *sensor) { this->channel_z_sensor_ = sensor; }
  void set_channel_w_sensor(sensor::Sensor *sensor) { this->channel_w_sensor_ = sensor; }

#ifdef USE_BINARY_SENSOR
  void set_conversion_ready_binary_sensor(binary_sensor::BinarySensor *sensor) {
    this->conversion_ready_binary_sensor_ = sensor;
  }
  void set_overload_binary_sensor(binary_sensor::BinarySensor *sensor) { this->overload_binary_sensor_ = sensor; }
  void set_threshold_low_binary_sensor(binary_sensor::BinarySensor *sensor) {
    this->threshold_low_binary_sensor_ = sensor;
  }
  void set_threshold_high_binary_sensor(binary_sensor::BinarySensor *sensor) {
    this->threshold_high_binary_sensor_ = sensor;
  }
#endif

 protected:
  bool read_u16_(uint8_t a_register, uint16_t &value);
  bool write_u16_(uint8_t a_register, uint16_t value);
  bool write_config_();
  bool write_threshold_cfg_();
  bool write_threshold_(uint8_t a_register, uint32_t adc_code);
  bool read_channels_(uint32_t channels[4]);
  bool crc_ok_(uint8_t exp, uint32_t mantissa, uint8_t counter, uint8_t crc) const;
  uint32_t conversion_timeout_ms_() const;
  void read_and_publish_();

  OPT4048Range range_{OPT4048Range::OPT4048_RANGE_AUTO};
  OPT4048ConversionTime conversion_time_{OPT4048ConversionTime::OPT4048_CONVERSION_TIME_100MS};
  OPT4048Mode mode_{OPT4048Mode::OPT4048_MODE_ONESHOT};
  OPT4048FaultCount fault_count_{OPT4048FaultCount::OPT4048_FAULT_COUNT_1};
  OPT4048IntConfig interrupt_config_{OPT4048IntConfig::OPT4048_INT_CONFIG_DATA_READY_ALL};
  uint8_t threshold_channel_{1};
  bool quick_wake_{false};
  bool interrupt_latch_{true};
  bool interrupt_polarity_high_{true};
  bool interrupt_direction_high_{true};
  bool has_threshold_low_{false};
  bool has_threshold_high_{false};
  uint32_t threshold_low_{0};
  uint32_t threshold_high_{0};
  bool updating_{false};

  GPIOPin *interrupt_pin_{nullptr};
  sensor::Sensor *illuminance_sensor_{nullptr};
  sensor::Sensor *x_sensor_{nullptr};
  sensor::Sensor *y_sensor_{nullptr};
  sensor::Sensor *color_temperature_sensor_{nullptr};
  sensor::Sensor *channel_x_sensor_{nullptr};
  sensor::Sensor *channel_y_sensor_{nullptr};
  sensor::Sensor *channel_z_sensor_{nullptr};
  sensor::Sensor *channel_w_sensor_{nullptr};

#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *conversion_ready_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *overload_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *threshold_low_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *threshold_high_binary_sensor_{nullptr};
#endif
};

}  // namespace esphome::opt4048
