#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"

#include <vector>

namespace esphome {
namespace ade7953_base {

static const uint8_t PGA_V_8 =
    0x007;  // PGA_V,  (R/W) Default: 0x00, Unsigned, Voltage channel gain configuration (Bits[2:0])
static const uint8_t PGA_IA_8 =
    0x008;  // PGA_IA, (R/W) Default: 0x00, Unsigned, Current Channel A gain configuration (Bits[2:0])
static const uint8_t PGA_IB_8 =
    0x009;  // PGA_IB, (R/W) Default: 0x00, Unsigned, Current Channel B gain configuration (Bits[2:0])

static const uint32_t AIGAIN_32 =
    0x380;  // AIGAIN, (R/W)   Default: 0x400000, Unsigned,Current channel gain (Current Channel A)(32 bit)
static const uint32_t AVGAIN_32 = 0x381;  // AVGAIN, (R/W)   Default: 0x400000, Unsigned,Voltage channel gain(32 bit)
static const uint32_t AWGAIN_32 =
    0x382;  // AWGAIN, (R/W)   Default: 0x400000, Unsigned,Active power gain (Current Channel A)(32 bit)
static const uint32_t AVARGAIN_32 =
    0x383;  // AVARGAIN, (R/W) Default: 0x400000, Unsigned, Reactive power gain (Current Channel A)(32 bit)
static const uint32_t AVAGAIN_32 =
    0x384;  // AVAGAIN, (R/W)  Default: 0x400000, Unsigned,Apparent power gain (Current Channel A)(32 bit)

static const uint32_t BIGAIN_32 =
    0x38C;  // BIGAIN, (R/W)   Default: 0x400000, Unsigned,Current channel gain (Current Channel B)(32 bit)
static const uint32_t BVGAIN_32 = 0x38D;  // BVGAIN, (R/W)   Default: 0x400000, Unsigned,Voltage channel gain(32 bit)
static const uint32_t BWGAIN_32 =
    0x38E;  // BWGAIN, (R/W)   Default: 0x400000, Unsigned,Active power gain (Current Channel B)(32 bit)
static const uint32_t BVARGAIN_32 =
    0x38F;  // BVARGAIN, (R/W) Default: 0x400000, Unsigned, Reactive power gain (Current Channel B)(32 bit)
static const uint32_t BVAGAIN_32 =
    0x390;  // BVAGAIN, (R/W)  Default: 0x400000, Unsigned,Apparent power gain (Current Channel B)(32 bit)

class ADE7953 : public PollingComponent, public sensor::Sensor {
 public:
  void set_irq_pin(InternalGPIOPin *irq_pin) { irq_pin_ = irq_pin; }

  // Set PGA input gains: 0 1x, 1 2x, 0b10 4x
  void set_pga_v(uint8_t pga_v) { pga_v_ = pga_v; }
  void set_pga_ia(uint8_t pga_ia) { pga_ia_ = pga_ia; }
  void set_pga_ib(uint8_t pga_ib) { pga_ib_ = pga_ib; }

  // Set input gains
  void set_vgain(uint32_t vgain) { vgain_ = vgain; }
  void set_aigain(uint32_t aigain) { aigain_ = aigain; }
  void set_bigain(uint32_t bigain) { bigain_ = bigain; }
  void set_awgain(uint32_t awgain) { awgain_ = awgain; }
  void set_bwgain(uint32_t bwgain) { bwgain_ = bwgain; }

  void set_use_acc_energy_regs(bool use_acc_energy_regs) { use_acc_energy_regs_ = use_acc_energy_regs; }

  void set_voltage_sensor(sensor::Sensor *voltage_sensor) { voltage_sensor_ = voltage_sensor; }
  void set_frequency_sensor(sensor::Sensor *frequency_sensor) { frequency_sensor_ = frequency_sensor; }

  void set_power_factor_a_sensor(sensor::Sensor *power_factor_a) { power_factor_a_sensor_ = power_factor_a; }
  void set_power_factor_b_sensor(sensor::Sensor *power_factor_b) { power_factor_b_sensor_ = power_factor_b; }

  void set_current_a_sensor(sensor::Sensor *current_a_sensor) { current_a_sensor_ = current_a_sensor; }
  void set_current_b_sensor(sensor::Sensor *current_b_sensor) { current_b_sensor_ = current_b_sensor; }

  void set_apparent_power_a_sensor(sensor::Sensor *apparent_power_a) { apparent_power_a_sensor_ = apparent_power_a; }
  void set_apparent_power_b_sensor(sensor::Sensor *apparent_power_b) { apparent_power_b_sensor_ = apparent_power_b; }

  void set_active_power_a_sensor(sensor::Sensor *active_power_a_sensor) {
    active_power_a_sensor_ = active_power_a_sensor;
  }
  void set_active_power_b_sensor(sensor::Sensor *active_power_b_sensor) {
    active_power_b_sensor_ = active_power_b_sensor;
  }

  void set_reactive_power_a_sensor(sensor::Sensor *reactive_power_a) { reactive_power_a_sensor_ = reactive_power_a; }
  void set_reactive_power_b_sensor(sensor::Sensor *reactive_power_b) { reactive_power_b_sensor_ = reactive_power_b; }

  void setup() override;

  void dump_config() override;

  void update() override;

 protected:
  InternalGPIOPin *irq_pin_{nullptr};
  bool is_setup_{false};
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *frequency_sensor_{nullptr};
  sensor::Sensor *current_a_sensor_{nullptr};
  sensor::Sensor *current_b_sensor_{nullptr};
  sensor::Sensor *apparent_power_a_sensor_{nullptr};
  sensor::Sensor *apparent_power_b_sensor_{nullptr};
  sensor::Sensor *active_power_a_sensor_{nullptr};
  sensor::Sensor *active_power_b_sensor_{nullptr};
  sensor::Sensor *reactive_power_a_sensor_{nullptr};
  sensor::Sensor *reactive_power_b_sensor_{nullptr};
  sensor::Sensor *power_factor_a_sensor_{nullptr};
  sensor::Sensor *power_factor_b_sensor_{nullptr};
  uint8_t pga_v_;
  uint8_t pga_ia_;
  uint8_t pga_ib_;
  uint32_t vgain_;
  uint32_t aigain_;
  uint32_t bigain_;
  uint32_t awgain_;
  uint32_t bwgain_;
  bool use_acc_energy_regs_{false};
  uint32_t last_update_;

  virtual bool ade_write_8(uint16_t reg, uint8_t value) = 0;

  virtual bool ade_write_16(uint16_t reg, uint16_t value) = 0;

  virtual bool ade_write_32(uint16_t reg, uint32_t value) = 0;

  virtual bool ade_read_8(uint16_t reg, uint8_t *value) = 0;

  virtual bool ade_read_16(uint16_t reg, uint16_t *value) = 0;

  virtual bool ade_read_32(uint16_t reg, uint32_t *value) = 0;
};

}  // namespace ade7953_base
}  // namespace esphome
