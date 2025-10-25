#pragma once

#include "esphome/core/component.h"
#include "esphome/core/datatypes.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace bl0942 {

// The BL0942 IC is "calibration-free", which means that it doesn't care
// at all about calibration, and that's left to software. It measures a
// voltage differential on its IP/IN pins which linearly proportional to
// the current flow, and another on its VP pin which is proportional to
// the line voltage. It never knows the actual calibration; the values
// it reports are solely in terms of those inputs.
//
// The datasheet refers to the input voltages as I(A) and V(V), both
// in millivolts. It measures them against a reference voltage Vref,
// which is typically 1.218V (but that absolute value is meaningless
// without the actual calibration anyway).
//
// The reported I_RMS value is 305978 I(A)/Vref, and the reported V_RMS
// value is 73989 V(V)/Vref. So we can calibrate those by applying a
// simple meter with a resistive load.
//
// The chip also measures the phase difference between voltage and
// current, and uses it to calculate the power factor (cos φ). It
// reports the WATT value of 3537 * I_RMS * V_RMS * cos φ).
//
// It also integrates total energy based on the WATT value. The time for
// one CF_CNT pulse is 1638.4*256 / WATT.
//
// So... how do we calibrate that?
//
// Using a simple resistive load and an external meter, we can measure
// the true voltage and current for a given V_RMS and I_RMS reading,
// to calculate BL0942_UREF and BL0942_IREF. Those are in units of
// "305978 counts per amp" or "73989 counts per volt" respectively.
//
// We can derive BL0942_PREF from those. Let's eliminate the weird
// factors and express the calibration in plain counts per volt/amp:
// UREF1 = UREF/73989, IREF1 = IREF/305978.
//
// Next... the true power in Watts is V * I * cos φ, so that's equal
// to WATT/3537 * IREF1 * UREF1. Which means
// BL0942_PREF = BL0942_UREF * BL0942_IREF * 3537 / 305978 / 73989.
//
// Finally the accumulated energy. The period of a CF_CNT count is
// 1638.4*256 / WATT seconds, or 419230.4 / WATT seconds. Which means
// the energy represented by a CN_CNT pulse is 419230.4 WATT-seconds.
// Factoring in the calibration, that's 419230.4 / BL0942_PREF actual
// Watt-seconds (or Joules, as the physicists like to call them).
//
// But we're not being physicists today; we we're being engineers, so
// we want to convert to kWh instead. Which we do by dividing by 1000
// and then by 3600, so the energy in kWh is
// CF_CNT * 419230.4 / BL0942_PREF / 3600000
//
// Which makes BL0952_EREF = BL0942_PREF * 3600000 / 419430.4

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
  void set_reset(bool reset) { this->reset_ = reset; }
  void set_current_reference(float current_ref) {
    this->current_reference_ = current_ref;
    this->current_reference_set_ = true;
  }
  void set_energy_reference(float energy_ref) {
    this->energy_reference_ = energy_ref;
    this->energy_reference_set_ = true;
  }
  void set_power_reference(float power_ref) {
    this->power_reference_ = power_ref;
    this->power_reference_set_ = true;
  }
  void set_voltage_reference(float voltage_ref) {
    this->voltage_reference_ = voltage_ref;
    this->voltage_reference_set_ = true;
  }

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
  bool power_reference_set_ = false;
  // Divide by this to turn into Volt
  float voltage_reference_ = BL0942_UREF;
  bool voltage_reference_set_ = false;
  // Divide by this to turn into Ampere
  float current_reference_ = BL0942_IREF;
  bool current_reference_set_ = false;
  // Divide by this to turn into kWh
  float energy_reference_ = BL0942_EREF;
  bool energy_reference_set_ = false;
  uint8_t address_ = 0;
  bool reset_ = false;
  LineFrequency line_freq_ = LINE_FREQUENCY_50HZ;
  uint32_t rx_start_ = 0;
  uint32_t prev_cf_cnt_ = 0;

  bool validate_checksum_(DataPacket *data);
  int read_reg_(uint8_t reg);
  void write_reg_(uint8_t reg, uint32_t val);
  void received_package_(DataPacket *data);
};
}  // namespace bl0942
}  // namespace esphome
