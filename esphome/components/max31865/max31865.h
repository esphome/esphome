#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace max31865 {

enum MAX31865RegisterMasks { SPI_WRITE_M = 0x80 };
enum MAX31865Registers {
  CONFIGURATION_REG = 0x00,
  RTD_RESISTANCE_MSB_REG = 0x01,
  RTD_RESISTANCE_LSB_REG = 0x02,
  FAULT_THRESHOLD_H_MSB_REG = 0x03,
  FAULT_THRESHOLD_H_LSB_REG = 0x04,
  FAULT_THRESHOLD_L_MSB_REG = 0x05,
  FAULT_THRESHOLD_L_LSB_REG = 0x06,
  FAULT_STATUS_REG = 0x07,
};
enum MAX31865ConfigFilter {
  FILTER_60HZ = 0,
  FILTER_50HZ = 1,
};

class MAX31865Sensor : public sensor::Sensor,
                       public PollingComponent,
                       public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                             spi::CLOCK_PHASE_TRAILING, spi::DATA_RATE_4MHZ> {
 public:
  void set_reference_resistance(float reference_resistance) { reference_resistance_ = reference_resistance; }
  void set_nominal_resistance(float nominal_resistance) { rtd_nominal_resistance_ = nominal_resistance; }
  void set_filter(MAX31865ConfigFilter filter) { filter_ = filter; }
  void set_num_rtd_wires(uint8_t rtd_wires) { rtd_wires_ = rtd_wires; }
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void update() override;

 protected:
  float reference_resistance_;
  float rtd_nominal_resistance_;
  MAX31865ConfigFilter filter_;
  uint8_t rtd_wires_;
  uint8_t base_config_;
  bool has_fault_ = false;
  bool has_warn_ = false;
  void read_data_();
  void write_config_(uint8_t mask, uint8_t bits, uint8_t start_position = 0);
  void write_register_(uint8_t reg, uint8_t value);
  uint8_t read_register_(uint8_t reg);
  uint16_t read_register_16_(uint8_t reg);
  float calc_temperature_(float rtd_ratio);
};

}  // namespace max31865
}  // namespace esphome
