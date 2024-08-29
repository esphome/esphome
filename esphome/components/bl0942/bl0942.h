#pragma once

#include "esphome/core/component.h"
#include "esphome/core/datatypes.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace bl0942 {

static const float BL0942_PREF = 596;              // taken from tasmota
static const float BL0942_UREF = 15873.35944299;   // should be 73989/1.218
static const float BL0942_IREF = 251213.46469622;  // 305978/1.218
static const float BL0942_EREF = 3304.61127328;    // Measured

struct DataPacket {
  uint8_t frame_header;
  uint24_le_t i_rms;
  uint24_le_t v_rms;
  uint24_le_t i_fast_rms;
  int24_le_t watt;
  uint24_le_t cf_cnt;
  uint16_le_t frequency;
  uint8_t reserved1;
  uint8_t status;
  uint8_t reserved2;
  uint8_t reserved3;
  uint8_t checksum;
} __attribute__((packed));

enum LineFrequency : uint8_t {
  LINE_FREQUENCY_50HZ = 50,
  LINE_FREQUENCY_60HZ = 60,
};

class BL0942 : public PollingComponent, public uart::UARTDevice {
 public:
  void set_voltage_sensor(sensor::Sensor *voltage_sensor) { voltage_sensor_ = voltage_sensor; }
  void set_current_sensor(sensor::Sensor *current_sensor) { current_sensor_ = current_sensor; }
  void set_power_sensor(sensor::Sensor *power_sensor) { power_sensor_ = power_sensor; }
  void set_energy_sensor(sensor::Sensor *energy_sensor) { energy_sensor_ = energy_sensor; }
  void set_frequency_sensor(sensor::Sensor *frequency_sensor) { frequency_sensor_ = frequency_sensor; }
  void set_line_freq(LineFrequency freq) { this->line_freq_ = freq; }
  void set_address(uint8_t address) { this->address_ = address; }

  void loop() override;
  void update() override;
  void setup() override;
  void dump_config() override;

 protected:
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *current_sensor_{nullptr};
  // NB This may be negative as the circuits is seemingly able to measure
  // power in both directions
  sensor::Sensor *power_sensor_{nullptr};
  sensor::Sensor *energy_sensor_{nullptr};
  sensor::Sensor *frequency_sensor_{nullptr};

  // Divide by this to turn into Watt
  float power_reference_ = BL0942_PREF;
  // Divide by this to turn into Volt
  float voltage_reference_ = BL0942_UREF;
  // Divide by this to turn into Ampere
  float current_reference_ = BL0942_IREF;
  // Divide by this to turn into kWh
  float energy_reference_ = BL0942_EREF;
  uint8_t address_ = 0;
  LineFrequency line_freq_ = LINE_FREQUENCY_50HZ;

  bool validate_checksum_(DataPacket *data);
  int read_reg_(uint8_t reg);
  void write_reg_(uint8_t reg, uint32_t val);
  void received_package_(DataPacket *data);
};
}  // namespace bl0942
}  // namespace esphome
