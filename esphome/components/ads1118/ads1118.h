#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/voltage_sampler/voltage_sampler.h"

namespace esphome {
namespace ads1118 {

enum ADS1118Multiplexer {
  ADS1118_MULTIPLEXER_P0_N1 = 0b000,
  ADS1118_MULTIPLEXER_P0_N3 = 0b001,
  ADS1118_MULTIPLEXER_P1_N3 = 0b010,
  ADS1118_MULTIPLEXER_P2_N3 = 0b011,
  ADS1118_MULTIPLEXER_P0_NG = 0b100,
  ADS1118_MULTIPLEXER_P1_NG = 0b101,
  ADS1118_MULTIPLEXER_P2_NG = 0b110,
  ADS1118_MULTIPLEXER_P3_NG = 0b111,
};

enum ADS1118Gain {
  ADS1118_GAIN_6P144 = 0b000,
  ADS1118_GAIN_4P096 = 0b001,
  ADS1118_GAIN_2P048 = 0b010,
  ADS1118_GAIN_1P024 = 0b011,
  ADS1118_GAIN_0P512 = 0b100,
  ADS1118_GAIN_0P256 = 0b101,
};

class ADS1118Sensor;

class ADS1118 : public Component,
                public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_TRAILING,
                                      spi::DATA_RATE_1MHZ> {
 public:
  ADS1118() = default;
  void register_sensor(ADS1118Sensor *obj) { this->sensors_.push_back(obj); }
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  /// Helper method to request a measurement from a sensor.
  float request_measurement(ADS1118Sensor *sensor);
 protected:
  uint16_t config{0};
  std::vector<ADS1118Sensor *> sensors_;
};

class ADS1118Sensor : public PollingComponent, public sensor::Sensor, public voltage_sampler::VoltageSampler {
 public:
  ADS1118Sensor(ADS1118 *parent) : parent_(parent) {}

  void update() override;
  void set_multiplexer(ADS1118Multiplexer multiplexer) { multiplexer_ = multiplexer; }
  void set_gain(ADS1118Gain gain) { gain_ = gain; }
  void set_temperature_mode(bool temp) { temperature_mode_ = temp; }
  uint8_t get_multiplexer() const { return multiplexer_; }
  uint8_t get_gain() const { return gain_; }
  bool get_temperature_mode() const { return temperature_mode_; }
  float sample() override;

 protected:
  ADS1118 *parent_;
  ADS1118Multiplexer multiplexer_;
  ADS1118Gain gain_;
  bool temperature_mode_;
};

}  // namespace ads1118
}  // namespace esphome
