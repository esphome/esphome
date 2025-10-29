#pragma once

#include "esphome/core/component.h"
#include "esphome/core/datatypes.h"
#include "esphome/core/defines.h"
#ifdef USE_BUTTON
#include "esphome/components/button/button.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace bl0940 {

// Caveat: All these values are big endian (low - middle - high)
struct DataPacket {
  uint8_t frame_header;    // Packet header (0x58 in EN docs, 0x55 in CN docs and Tasmota tests)
  uint24_le_t i_fast_rms;  // Fast RMS current
  uint24_le_t i_rms;       // RMS current
  uint24_t RESERVED0;      // Reserved
  uint24_le_t v_rms;       // RMS voltage
  uint24_t RESERVED1;      // Reserved
  int24_le_t watt;         // Active power (can be negative for bidirectional measurement)
  uint24_t RESERVED2;      // Reserved
  uint24_le_t cf_cnt;      // Energy pulse count
  uint24_t RESERVED3;      // Reserved
  uint16_le_t tps1;        // Internal temperature sensor 1
  uint8_t RESERVED4;       // Reserved (should be 0x00)
  uint16_le_t tps2;        // Internal temperature sensor 2
  uint8_t RESERVED5;       // Reserved (should be 0x00)
  uint8_t checksum;        // Packet checksum
} __attribute__((packed));

class BL0940 : public PollingComponent, public uart::UARTDevice {
 public:
  // Sensor setters
  void set_voltage_sensor(sensor::Sensor *voltage_sensor) { voltage_sensor_ = voltage_sensor; }
  void set_current_sensor(sensor::Sensor *current_sensor) { current_sensor_ = current_sensor; }
  void set_power_sensor(sensor::Sensor *power_sensor) { power_sensor_ = power_sensor; }
  void set_energy_sensor(sensor::Sensor *energy_sensor) { energy_sensor_ = energy_sensor; }

  // Temperature sensor setters
  void set_internal_temperature_sensor(sensor::Sensor *internal_temperature_sensor) {
    internal_temperature_sensor_ = internal_temperature_sensor;
  }
  void set_external_temperature_sensor(sensor::Sensor *external_temperature_sensor) {
    external_temperature_sensor_ = external_temperature_sensor;
  }

  // Configuration setters
  void set_legacy_mode(bool enable) { this->legacy_mode_enabled_ = enable; }
  void set_read_command(uint8_t read_command) { this->read_command_ = read_command; }
  void set_write_command(uint8_t write_command) { this->write_command_ = write_command; }

  // Reference value setters (used for calibration and conversion)
  void set_current_reference(float current_ref) { this->current_reference_ = current_ref; }
  void set_energy_reference(float energy_ref) { this->energy_reference_ = energy_ref; }
  void set_power_reference(float power_ref) { this->power_reference_ = power_ref; }
  void set_voltage_reference(float voltage_ref) { this->voltage_reference_ = voltage_ref; }

#ifdef USE_NUMBER
  // Calibration number setters (for Home Assistant number entities)
  void set_current_calibration_number(number::Number *num) { this->current_calibration_number_ = num; }
  void set_voltage_calibration_number(number::Number *num) { this->voltage_calibration_number_ = num; }
  void set_power_calibration_number(number::Number *num) { this->power_calibration_number_ = num; }
  void set_energy_calibration_number(number::Number *num) { this->energy_calibration_number_ = num; }
#endif

#ifdef USE_BUTTON
  // Resets all calibration values to defaults (can be triggered by a button)
  void reset_calibration();
#endif

  // Core component methods
  void loop() override;
  void update() override;
  void setup() override;
  void dump_config() override;

 protected:
  // --- Sensor pointers ---
  sensor::Sensor *voltage_sensor_{nullptr};               // Voltage sensor
  sensor::Sensor *current_sensor_{nullptr};               // Current sensor
  sensor::Sensor *power_sensor_{nullptr};                 // Power sensor (can be negative for bidirectional)
  sensor::Sensor *energy_sensor_{nullptr};                // Energy sensor
  sensor::Sensor *internal_temperature_sensor_{nullptr};  // Internal temperature sensor
  sensor::Sensor *external_temperature_sensor_{nullptr};  // External temperature sensor

#ifdef USE_NUMBER
  // --- Calibration number entities (for dynamic calibration via HA UI) ---
  number::Number *voltage_calibration_number_{nullptr};
  number::Number *current_calibration_number_{nullptr};
  number::Number *power_calibration_number_{nullptr};
  number::Number *energy_calibration_number_{nullptr};
#endif

  // --- Internal state ---
  uint32_t prev_cf_cnt_ = 0;       // Previous energy pulse count (for energy calculation)
  float max_temperature_diff_{0};  // Max allowed temperature difference between two measurements (noise filter)

  // --- Reference values for conversion ---
  float power_reference_;        // Divider for raw power to get Watts
  float power_reference_cal_;    // Calibrated power reference
  float voltage_reference_;      // Divider for raw voltage to get Volts
  float voltage_reference_cal_;  // Calibrated voltage reference
  float current_reference_;      // Divider for raw current to get Amperes
  float current_reference_cal_;  // Calibrated current reference
  float energy_reference_;       // Divider for raw energy to get kWh
  float energy_reference_cal_;   // Calibrated energy reference

  // --- Home Assistant calibration values (multipliers, default 1) ---
  float current_cal_{1};
  float voltage_cal_{1};
  float power_cal_{1};
  float energy_cal_{1};

  // --- Protocol commands ---
  uint8_t read_command_;
  uint8_t write_command_;

  // --- Mode flags ---
  bool legacy_mode_enabled_ = true;

  // --- Methods ---
  // Converts packed temperature value to float and updates the sensor
  float update_temp_(sensor::Sensor *sensor, uint16_le_t packed_temperature) const;

  // Validates the checksum of a received data packet
  bool validate_checksum_(DataPacket *data);

  // Handles a received data packet
  void received_package_(DataPacket *data);

  // Calculates reference values for calibration and conversion
  float calculate_energy_reference_();
  float calculate_power_reference_();
  float calculate_calibration_value_(float state);

  // Calibration update callbacks (used with number entities)
  void current_calibration_callback_(float state);
  void voltage_calibration_callback_(float state);
  void power_calibration_callback_(float state);
  void energy_calibration_callback_(float state);
  void reset_calibration_callback_();

  // Recalculates all reference values after calibration changes
  void recalibrate_();
};

}  // namespace bl0940
}  // namespace esphome
