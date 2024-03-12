#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace bh1745 {

enum class Bh1745Registers : uint8_t {
  SYSTEM_CONTROL = 0x40,
  MODE_CONTROL1 = 0x41,
  MODE_CONTROL2 = 0x42,
  MODE_CONTROL3 = 0x44,
  RED_DATA_LSB = 0x50,
  RED_DATA_MSB = 0x51,
  GREEN_DATA_LSB = 0x52,
  GREEN_DATA_MSB = 0x53,
  BLUE_DATA_LSB = 0x54,
  BLUE_DATA_MSB = 0x55,
  CLEAR_DATA_LSB = 0x56,
  CLEAR_DATA_MSB = 0x57,
  DINT_DATA_LSB = 0x58,
  DINT_DATA_MSB = 0x59,
  INTERRUPT_REG = 0x60,
  PERSISTENCE = 0x61,
  TH_LSB = 0x62,
  TH_MSB = 0x63,
  TL_LSB = 0x64,
  TL_MSB = 0x65,
  MANUFACTURER_ID = 0x92,
};

enum MeasurementTime : uint8_t {
  TIME_160MS = 0b000,
  TIME_320MS = 0b001,
  TIME_640MS = 0b010,
  TIME_1280MS = 0b011,
  TIME_2560MS = 0b100,
  TIME_5120MS = 0b101,
};

enum AdcGain : uint8_t {
  GAIN_1X = 0,
  GAIN_2X,
  GAIN_16X,
};

// 0x40
union SystemControlRegister {
  uint8_t raw;
  struct {
    uint8_t part_id : 6;
    uint8_t int_reset : 1;
    uint8_t sw_reset : 1;
  };
};

// 0x41
union ModeControl1Register {
  u_int8_t raw;
  struct {
    MeasurementTime measurement_time : 3;
    uint8_t reserved_3_7 : 5;
  };
};

// 0x42
union ModeControl2Register {
  u_int8_t raw;
  struct {
    AdcGain adc_gain : 2;
    uint8_t reserved_2_3 : 2;
    bool rgbc_measurement_enable : 1;
    uint8_t reserved_5_6 : 2;
    bool valid : 1;
  };
};

constexpr uint8_t BH1745_CHANNELS = 4;

// 0x44
// always write 0x02

class BH1745Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;
  float get_setup_priority() const override;

  void set_measurement_time(MeasurementTime measurement_time) { this->measurement_time_ = measurement_time; };
  void set_adc_gain(AdcGain adc_gain) { this->adc_gain_ = adc_gain; };
  void set_glass_attenuation_factor(float factor) { this->glass_attenuation_factor_ = factor; }

  void set_red_counts_sensor(sensor::Sensor *red) { this->red_counts_sensor_ = red; }
  void set_green_counts_sensor(sensor::Sensor *green) { this->green_counts_sensor_ = green; }
  void set_blue_counts_sensor(sensor::Sensor *blue) { this->blue_counts_sensor_ = blue; }
  void set_clear_counts_sensor(sensor::Sensor *clear) { this->clear_counts_sensor_ = clear; }
  void set_illuminance_sensor(sensor::Sensor *illuminance) { this->illuminance_sensor_ = illuminance; }
  void set_color_temperature_sensor(sensor::Sensor *cct) { this->color_temperature_sensor_ = cct; }

  // only for Pimoroni board
  void switch_led(bool on_off);

 protected:
  MeasurementTime measurement_time_{MeasurementTime::TIME_160MS};
  AdcGain adc_gain_{AdcGain::GAIN_1X};
  float glass_attenuation_factor_{1.0};

  sensor::Sensor *red_counts_sensor_{nullptr};
  sensor::Sensor *green_counts_sensor_{nullptr};
  sensor::Sensor *blue_counts_sensor_{nullptr};
  sensor::Sensor *clear_counts_sensor_{nullptr};
  sensor::Sensor *illuminance_sensor_{nullptr};
  sensor::Sensor *color_temperature_sensor_{nullptr};

  enum class State : uint8_t {
    NOT_INITIALIZED,
    DELAYED_SETUP,
    IDLE,
    MEASUREMENT_IN_PROGRESS,
    WAITING_FOR_DATA,
    DATA_COLLECTED,
  } state_{State::NOT_INITIALIZED};

  struct Readings {
    uint16_t red;
    uint16_t green;
    uint16_t blue;
    uint16_t clear;

    AdcGain gain;
    MeasurementTime meas_time;

    uint8_t tries;
  } readings_;

  void configure_measurement_time_();
  void configure_gain_();

  bool is_data_ready_(Readings &data);
  void read_data_(Readings &data);

  float calculate_lux_(Readings &data);
  float calculate_cct_(Readings &data);

  void publish_data_();
};

}  // namespace bh1745
}  // namespace esphome
