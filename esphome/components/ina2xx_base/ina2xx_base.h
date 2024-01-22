#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace ina2xx_base {

enum RegisterMap : uint8_t {
  REG_CONFIG = 0x00,
  REG_ADC_CONFIG = 0x01,
  REG_SHUNT_CAL = 0x02,
  REG_SHUNT_TEMPCO = 0x03,
  REG_VSHUNT = 0x04,
  REG_VBUS = 0x05,
  REG_DIETEMP = 0x06,
  REG_CURRENT = 0x07,
  REG_POWER = 0x08,
  REG_ENERGY = 0x09,
  REG_CHARGE = 0x0A,
  REG_DIAG_ALRT = 0x0B,
  REG_SOVL = 0x0C,
  REG_SUVL = 0x0D,
  REG_BOVL = 0x0E,
  REG_BUVL = 0x0F,
  REG_TEMP_LIMIT = 0x10,
  REG_PWR_LIMIT = 0x11,
  REG_MANUFACTURER_ID = 0x3E,
  REG_DEVICE_ID = 0x3F
};

enum AdcRange : uint8_t {
  ADC_RANGE_0 = 0,
  ADC_RANGE_1 = 1,
};

enum AdcSpeed : uint8_t {
  ADC_SPEED_50US = 0,
  ADC_SPEED_84US = 1,
  ADC_SPEED_150US = 2,
  ADC_SPEED_280US = 3,
  ADC_SPEED_540US = 4,
  ADC_SPEED_1052US = 5,
  ADC_SPEED_2074US = 6,
  ADC_SPEED_4120US = 7,
};

enum AdcSample : uint8_t {
  ADC_SAMPLE_1 = 0,
  ADC_SAMPLE_4 = 1,
  ADC_SAMPLE_16 = 2,
  ADC_SAMPLE_64 = 3,
  ADC_SAMPLE_128 = 4,
  ADC_SAMPLE_256 = 5,
  ADC_SAMPLE_512 = 6,
  ADC_SAMPLE_1024 = 7,
};

union ConfigurationRegister {
  uint16_t raw_u16;
  struct {
    uint8_t reserved_0_3 : 4;  // Reserved
    AdcRange ADCRANGE : 1;     // Shunt measurement range 0: ±163.84 mV, 1: ±40.96 mV
    bool TEMPCOMP : 1;         // Temperature compensation enable
    uint8_t CONVDLY : 8;       // Sets the Delay for initial ADC conversion in steps of 2 ms.
    bool RSTACC : 1;           // Reset counters
    bool RST : 1;              // Full device reset
  } __attribute__((packed));
};

union AdcConfigurationRegister {
  uint16_t raw_u16;
  struct {
    AdcSample AVG : 3;
    AdcSpeed VTCT : 3;
    AdcSpeed VSHCT : 3;
    AdcSpeed VBUSCT : 3;
    uint8_t MODE : 4;
  } __attribute__((packed));
};

union TempCompensationRegister {
  uint16_t raw_u16;
  struct {
    uint16_t TEMPCO : 14;
    uint16_t reserved : 2;
  } __attribute__((packed));
};

union DiagnosticRegister {
  uint16_t raw_u16;
  struct {
    bool MEMSTAT : 1;
    bool CNVRF : 1;
    bool POL : 1;
    bool BUSUL : 1;
    bool BUSOL : 1;
    bool SHNTUL : 1;
    bool SHNTOL : 1;
    bool TMPOL : 1;
    bool RESERVED1 : 1;
    bool MATHOF : 1;
    bool CHARGEOF : 1;
    bool ENERGYOF : 1;
    bool APOL : 1;
    bool SLOWALERT : 1;
    bool CNVR : 1;
    bool ALATCH : 1;
  } __attribute__((packed));
};

enum INAType : uint8_t {
  INA_20_BIT = 0,  // 228, 229
  INA_16_BIT = 1   // 237, 238, 239
};

struct INAParams {
  double vbus_lsb;
  double vshunt_lsb_range0;
  double vshunt_lst_range1;
  double shunt_scale;
  double lsb_divisor;
  double die_temp_lsb;
  double power_scale;
  bool accumulators;
  bool adc20bit;
  bool die16bit;
};

const INAParams INA_MODELS[2] = {
    {0.0001953125, 0.0003125, 0.000078125, 13107.2 * 1000000, 2 << 19, 0.0078125, 3.2, true, true, true},
    {0.0031250000, 0.0050000, 0.001250000, 819.2 * 1000000, 2 << 15, 0.1250000, 0.2, false, false, false}};

class INA2XX : public PollingComponent {
 public:
  void setup() override;
  float get_setup_priority() const override;
  void update() override;

  void set_shunt_resistance_ohm(float shunt_resistance_ohm) { shunt_resistance_ohm_ = shunt_resistance_ohm; }
  void set_max_current_a(float max_current_a) { max_current_a_ = max_current_a; }
  void set_adc_range(uint8_t range) { adc_range_ = (range == 0) ? AdcRange::ADC_RANGE_0 : AdcRange::ADC_RANGE_1; }

  void set_shunt_voltage_sensor(sensor::Sensor *sensor) { shunt_voltage_sensor_ = sensor; }
  void set_bus_voltage_sensor(sensor::Sensor *sensor) { bus_voltage_sensor_ = sensor; }
  void set_die_temperature_sensor(sensor::Sensor *sensor) { die_temperature_sensor_ = sensor; }
  void set_current_sensor(sensor::Sensor *sensor) { current_sensor_ = sensor; }
  void set_power_sensor(sensor::Sensor *sensor) { power_sensor_ = sensor; }
  void set_energy_sensor(sensor::Sensor *sensor) { energy_sensor_ = sensor; }
  void set_charge_sensor(sensor::Sensor *sensor) { charge_sensor_ = sensor; }

  bool reset_energy_counters();

 protected:
  bool reset_config_();

  bool configure_shunt_(double max_current, double r_shunt);
  bool configure_adc_range_();
  bool read_voltage_(double &volt_out);
  bool read_current_(double &amps_out);
  bool read_die_temp_(double &temp);
  bool read_volt_shunt_mv_(double &volt_out);
  bool read_power_(double &power_out);
  bool read_energy_(double &joules_out);
  bool read_charge_(double &coulombs_out);
  bool read_diagnostics_and_act_();

  uint64_t read_reg(uint8_t reg, uint8_t reg_size);

  bool read_u16_(uint8_t reg, uint16_t &out);
  bool read_u24_(uint8_t reg, uint32_t &out);

  bool read_s20_4_(uint8_t reg, int32_t &out);
  bool write_u16_(uint8_t reg, uint16_t val);

  double shunt_resistance_ohm_;
  double max_current_a_;
  AdcRange adc_range_{AdcRange::ADC_RANGE_0};
  uint16_t shunt_cal_{0};
  double current_lsb_{0};

  uint32_t energy_overflows_count_{0};
  uint32_t charge_overflows_count_{0};

  sensor::Sensor *shunt_voltage_sensor_{nullptr};
  sensor::Sensor *bus_voltage_sensor_{nullptr};
  sensor::Sensor *die_temperature_sensor_{nullptr};
  sensor::Sensor *current_sensor_{nullptr};
  sensor::Sensor *power_sensor_{nullptr};
  sensor::Sensor *energy_sensor_{nullptr};
  sensor::Sensor *charge_sensor_{nullptr};

  bool initialized{false};

  virtual bool read_2xx(uint8_t a_register, uint8_t *data, size_t len) = 0;
  virtual bool write_2xx(uint8_t a_register, const uint8_t *data, size_t len) = 0;
};

}  // namespace ina2xx_base
}  // namespace esphome
