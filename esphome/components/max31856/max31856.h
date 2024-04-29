#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/spi/spi.h"
#include "esphome/core/component.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

#include <cinttypes>

namespace esphome {
namespace max31856 {

/**
 * Multiple types of thermocouples supported by the chip.
 */
enum MAX31856ThermocoupleType : uint8_t {
  MAX31856_TCTYPE_B = 0b0000,  // 0x00
  MAX31856_TCTYPE_E = 0b0001,  // 0x01
  MAX31856_TCTYPE_J = 0b0010,  // 0x02
  MAX31856_TCTYPE_K = 0b0011,  // 0x03
  MAX31856_TCTYPE_N = 0b0100,  // 0x04
  MAX31856_TCTYPE_R = 0b0101,  // 0x05
  MAX31856_TCTYPE_S = 0b0110,  // 0x06
  MAX31856_TCTYPE_T = 0b0111,  // 0x07
};

enum MAX31856ConfigFilter : uint8_t {
  FILTER_60HZ = 0,
  FILTER_50HZ = 1,
};

enum MAX31856SamplesPerValue : uint8_t {
  AVE_SAMPLES_1 = 0 << 4,
  AVE_SAMPLES_2 = 1 << 4,
  AVE_SAMPLES_4 = 2 << 4,
  AVE_SAMPLES_8 = 3 << 4,
  AVE_SAMPLES_16 = 4 << 4,
};

class MAX31856Sensor : public sensor::Sensor,
                       public PollingComponent,
                       public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                             spi::CLOCK_PHASE_TRAILING, spi::DATA_RATE_4MHZ> {
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(has_fault)
#endif

 public:
  void setup() override;
  void loop() override;
  void update() override;
  void call_setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_type(MAX31856ThermocoupleType type) { type_ = type; }
  void set_filter(MAX31856ConfigFilter filter) { filter_ = filter; }
  void set_samples_per_value(MAX31856SamplesPerValue value) { samples_per_value_ = value; }
  void set_data_ready_pin(GPIOPin *pin) { data_ready_ = pin; }

 protected:
  MAX31856ThermocoupleType type_{};
  MAX31856ConfigFilter filter_{};
  MAX31856SamplesPerValue samples_per_value_{};
  GPIOPin *data_ready_{};
  uint8_t cr0_{};

  void read_oneshot_temperature_();
  void read_thermocouple_temperature_();
  bool have_faults_();

  uint8_t read_register_(uint8_t reg);
  uint16_t read_register16_(uint8_t reg);
  uint32_t read_register24_(uint8_t reg);
  void write_register_(uint8_t reg, uint8_t value);
  void write_bits_(uint8_t reg, uint8_t bits);
};

}  // namespace max31856
}  // namespace esphome
