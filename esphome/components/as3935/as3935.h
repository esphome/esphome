#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace as3935 {

enum AS3935RegisterNames {
  AFE_GAIN = 0x00,
  THRESHOLD,
  LIGHTNING_REG,
  INT_MASK_ANT,
  ENERGY_LIGHT_LSB,
  ENERGY_LIGHT_MSB,
  ENERGY_LIGHT_MMSB,
  DISTANCE,
  FREQ_DISP_IRQ,
  CALIB_TRCO = 0x3A,
  CALIB_SRCO = 0x3B,
  DEFAULT_RESET = 0x3C,
  CALIB_RCO = 0x3D
};

enum AS3935RegisterMasks {
  GAIN_MASK = 0x3E,
  SPIKE_MASK = 0xF,
  IO_MASK = 0xC1,
  DISTANCE_MASK = 0xC0,
  INT_MASK = 0xF0,
  THRESH_MASK = 0x0F,
  R_SPIKE_MASK = 0xF0,
  ENERGY_MASK = 0xF0,
  CAP_MASK = 0xF0,
  LIGHT_MASK = 0xCF,
  DISTURB_MASK = 0xDF,
  NOISE_FLOOR_MASK = 0x70,
  OSC_MASK = 0xE0,
  SPI_READ_M = 0x40,
  CALIB_MASK = 0x7F,
  DIV_MASK = 0x3F
};

enum AS3935Values {
  AS3935_ADDR = 0x03,
  INDOOR = 0x12,
  OUTDOOR = 0xE,
  LIGHTNING_INT = 0x08,
  DISTURBER_INT = 0x04,
  NOISE_INT = 0x01
};

/// Store data in a class that doesn't use multiple-inheritance (vtables in flash)
struct AS3935ComponentStore {
  volatile bool interrupt;

  ISRInternalGPIOPin *pin;
  static void gpio_intr(AS3935ComponentStore *arg);
};

class AS3935Component : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void loop() override;

  void set_pin(GPIOPin *pin) { pin_ = pin; }
  void set_distance_sensor(sensor::Sensor *distance_sensor) { distance_sensor_ = distance_sensor; }
  void set_energy_sensor(sensor::Sensor *energy_sensor) { energy_sensor_ = energy_sensor; }
  void set_thunder_alert_binary_sensor(binary_sensor::BinarySensor *thunder_alert_binary_sensor) {
    thunder_alert_binary_sensor_ = thunder_alert_binary_sensor;
  }
  void set_indoor(bool indoor);
  void set_watchdog_threshold(uint8_t sensitivity);
  void set_noise_level(uint8_t floor);
  void set_spike_rejection(uint8_t spike_sensitivity);
  void set_lightning_threshold(uint8_t strikes);
  void set_mask_disturber(bool enabled);
  void set_div_ratio(uint8_t div_ratio);
  void set_cap(uint8_t eight_pico_farad);

 protected:
  uint8_t read_interrupt_register_();
  void clear_statistics_();
  uint8_t get_distance_to_storm_();
  uint32_t get_lightning_energy_();

  void write_register_(uint8_t reg, uint8_t mask, uint8_t bits, uint8_t start_position);

  sensor::Sensor *distance_sensor_;
  sensor::Sensor *energy_sensor_;
  binary_sensor::BinarySensor *thunder_alert_binary_sensor_;
  GPIOPin *pin_;
  AS3935ComponentStore store_;
};

}  // namespace as3935
}  // namespace esphome
