#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::bl0939 {

// https://datasheet.lcsc.com/lcsc/2108071830_BL-Shanghai-Belling-BL0939_C2841044.pdf
// (unfortunately chinese, but the formulas can be easily understood)
// The datasheet has one set of formulas for direct mode (current shunt) and one for current transformer mode.

// Direct mode.
// Sonoff Dual R3 V2 has the exact same resistor values for the current shunts (RL=1miliOhm)
// and for the voltage divider (R1=0.51kOhm, R2=5*390kOhm)
// as in the manufacturer's reference circuit, so the same formulas were used here (Vref=1.218V)
static constexpr float BL0939_IREF = 324004 * 1 / 1.218;
static constexpr float BL0939_UREF = 79931 * 0.51 * 1000 / (1.218 * (5 * 390 + 0.51));
static constexpr float BL0939_PREF = 4046 * 1 * 0.51 * 1000 / (1.218 * 1.218 * (5 * 390 + 0.51));
static constexpr float BL0939_EREF = 3.6e6 * 4046 * 1 * 0.51 * 1000 / (1638.4 * 256 * 1.218 * 1.218 * (5 * 390 + 0.51));

// Current transformer mode.
// Values from the Seeed Studio XIAO 2-Channel Wi-Fi AC Energy Meter V1.0 schematic (Vref=1.218V).
// Current: 100A:50mA CT (2000:1) into a 0.6Ohm burden (2 * 0.3Ohm), so 0.3mV per primary amp.
// That is the same as a 0.3mOhm shunt in the direct mode formula.
// Voltage: ZMPT107-1 2mA:2mA voltage transformer with R2=5*22kOhm on the primary and R1=24.9Ohm on
// the secondary. The transformer isolates R1 from R2, so the divider is R1/R2 instead of R1/(R1+R2).
static constexpr float BL0939_IREF_CT = 324004 * 0.3 / 1.218;
static constexpr float BL0939_UREF_CT = 79931 * 24.9 * 1000 / (1.218 * 5 * 22000);
static constexpr float BL0939_PREF_CT = 4046 * 0.3 * 24.9 * 1000 / (1.218 * 1.218 * 5 * 22000);
static constexpr float BL0939_EREF_CT = 3.6e6 * 4046 * 0.3 * 24.9 * 1000 / (1638.4 * 256 * 1.218 * 1.218 * 5 * 22000);

enum class BL0939Mode : uint8_t {
  BL0939_MODE_DIRECT = 0,
  BL0939_MODE_CURRENT_TRANSFORMER = 1,
};

struct ube24_t {  // NOLINT(readability-identifier-naming,altera-struct-pack-align)
  uint8_t l;
  uint8_t m;
  uint8_t h;
} __attribute__((packed));

struct ube16_t {  // NOLINT(readability-identifier-naming,altera-struct-pack-align)
  uint8_t l;
  uint8_t h;
} __attribute__((packed));

struct sbe24_t {  // NOLINT(readability-identifier-naming,altera-struct-pack-align)
  uint8_t l;
  uint8_t m;
  int8_t h;
} __attribute__((packed));

// Caveat: All these values are big endian (low - middle - high)

union DataPacket {  // NOLINT(altera-struct-pack-align)
  uint8_t raw[35];
  struct {
    uint8_t frame_header;  // 0x55 according to docs
    ube24_t ia_fast_rms;
    ube24_t ia_rms;
    ube24_t ib_rms;
    ube24_t v_rms;
    ube24_t ib_fast_rms;
    sbe24_t a_watt;
    sbe24_t b_watt;
    sbe24_t cfa_cnt;
    sbe24_t cfb_cnt;
    ube16_t tps1;
    uint8_t RESERVED1;  // value of 0x00
    ube16_t tps2;
    uint8_t RESERVED2;  // value of 0x00
    uint8_t checksum;   // checksum
  };
} __attribute__((packed));

class BL0939 final : public PollingComponent, public uart::UARTDevice {
 public:
  void set_voltage_sensor(sensor::Sensor *voltage_sensor) { voltage_sensor_ = voltage_sensor; }
  void set_current_sensor_1(sensor::Sensor *current_sensor_1) { current_sensor_1_ = current_sensor_1; }
  void set_current_sensor_2(sensor::Sensor *current_sensor_2) { current_sensor_2_ = current_sensor_2; }
  void set_power_sensor_1(sensor::Sensor *power_sensor_1) { power_sensor_1_ = power_sensor_1; }
  void set_power_sensor_2(sensor::Sensor *power_sensor_2) { power_sensor_2_ = power_sensor_2; }
  void set_energy_sensor_1(sensor::Sensor *energy_sensor_1) { energy_sensor_1_ = energy_sensor_1; }
  void set_energy_sensor_2(sensor::Sensor *energy_sensor_2) { energy_sensor_2_ = energy_sensor_2; }
  void set_energy_sensor_sum(sensor::Sensor *energy_sensor_sum) { energy_sensor_sum_ = energy_sensor_sum; }
  void set_internal_temperature_sensor(sensor::Sensor *sensor) { this->internal_temperature_sensor_ = sensor; }
  // Inline so a constant mode from codegen folds down to plain stores.
  void set_mode(BL0939Mode mode) {
    this->mode_ = mode;
    const bool current_transformer = mode == BL0939Mode::BL0939_MODE_CURRENT_TRANSFORMER;
    this->power_reference_ = current_transformer ? BL0939_PREF_CT : BL0939_PREF;
    this->voltage_reference_ = current_transformer ? BL0939_UREF_CT : BL0939_UREF;
    this->current_reference_ = current_transformer ? BL0939_IREF_CT : BL0939_IREF;
    this->energy_reference_ = current_transformer ? BL0939_EREF_CT : BL0939_EREF;
  }

  void loop() override;

  void update() override;
  void setup() override;
  void dump_config() override;

 protected:
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *current_sensor_1_{nullptr};
  sensor::Sensor *current_sensor_2_{nullptr};
  // NB This may be negative as the circuits is seemingly able to measure
  // power in both directions
  sensor::Sensor *power_sensor_1_{nullptr};
  sensor::Sensor *power_sensor_2_{nullptr};
  sensor::Sensor *energy_sensor_1_{nullptr};
  sensor::Sensor *energy_sensor_2_{nullptr};
  sensor::Sensor *energy_sensor_sum_{nullptr};
  sensor::Sensor *internal_temperature_sensor_{nullptr};

  // Divide by this to turn into Watt
  float power_reference_ = BL0939_PREF;
  // Divide by this to turn into Volt
  float voltage_reference_ = BL0939_UREF;
  // Divide by this to turn into Ampere
  float current_reference_ = BL0939_IREF;
  // Divide by this to turn into kWh
  float energy_reference_ = BL0939_EREF;
  BL0939Mode mode_{BL0939Mode::BL0939_MODE_DIRECT};

  static uint32_t to_uint32_t(ube24_t input);

  static int32_t to_int32_t(sbe24_t input);

  static bool validate_checksum(const DataPacket *data);

  void received_package_(const DataPacket *data) const;
};
}  // namespace esphome::bl0939
