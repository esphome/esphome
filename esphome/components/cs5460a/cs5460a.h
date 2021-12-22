#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace cs5460a {

enum CS5460ACommand {
  CMD_SYNC0 = 0xfe,
  CMD_SYNC1 = 0xff,
  CMD_START_SINGLE = 0xe0,
  CMD_START_CONT = 0xe8,
  CMD_POWER_UP = 0xa0,
  CMD_POWER_STANDBY = 0x88,
  CMD_POWER_SLEEP = 0x90,
  CMD_CALIBRATION = 0xc0,
  CMD_READ = 0x00,
  CMD_WRITE = 0x40,
};

enum CS5460ARegister {
  REG_CONFIG = 0x00,
  REG_IDCOFF = 0x01,
  REG_IGN = 0x02,
  REG_VDCOFF = 0x03,
  REG_VGN = 0x04,
  REG_CYCLE_COUNT = 0x05,
  REG_PULSE_RATE = 0x06,
  REG_I = 0x07,
  REG_V = 0x08,
  REG_P = 0x09,
  REG_E = 0x0a,
  REG_IRMS = 0x0b,
  REG_VRMS = 0x0c,
  REG_TBC = 0x0d,
  REG_POFF = 0x0e,
  REG_STATUS = 0x0f,
  REG_IACOFF = 0x10,
  REG_VACOFF = 0x11,
  REG_MASK = 0x1a,
  REG_CONTROL = 0x1c,
};

/** Enum listing the current channel aplifiergain settings for the CS5460A.
 */
enum CS5460APGAGain {
  CS5460A_PGA_GAIN_10X = 0b0,
  CS5460A_PGA_GAIN_50X = 0b1,
};

class CS5460AComponent : public Component,
                         public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                               spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_1MHZ> {
 public:
  void set_samples(uint32_t samples) { samples_ = samples; }
  void set_phase_offset(int8_t phase_offset) { phase_offset_ = phase_offset; }
  void set_pga_gain(CS5460APGAGain pga_gain) { pga_gain_ = pga_gain; }
  void set_gains(float current_gain, float voltage_gain) {
    current_gain_ = current_gain;
    voltage_gain_ = voltage_gain;
  }
  void set_hpf_enable(bool current_hpf, bool voltage_hpf) {
    current_hpf_ = current_hpf;
    voltage_hpf_ = voltage_hpf;
  }
  void set_pulse_energy_wh(float pulse_energy_wh) { pulse_energy_wh_ = pulse_energy_wh; }
  void set_current_sensor(sensor::Sensor *current_sensor) { current_sensor_ = current_sensor; }
  void set_voltage_sensor(sensor::Sensor *voltage_sensor) { voltage_sensor_ = voltage_sensor; }
  void set_power_sensor(sensor::Sensor *power_sensor) { power_sensor_ = power_sensor; }

  void restart() { restart_(); }

  void setup() override;
  void loop() override {}
  float get_setup_priority() const override { return setup_priority::DATA; }
  void dump_config() override;

 protected:
  uint32_t samples_;
  int8_t phase_offset_;
  CS5460APGAGain pga_gain_;
  float current_gain_;
  float voltage_gain_;
  bool current_hpf_;
  bool voltage_hpf_;
  float pulse_energy_wh_;
  sensor::Sensor *current_sensor_{nullptr};
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *power_sensor_{nullptr};

  void write_register_(enum CS5460ARegister addr, uint32_t value);
  uint32_t read_register_(uint8_t addr);
  bool softreset_();
  void hw_init_();
  void restart_();
  void started_();
  void schedule_next_check_();
  bool check_status_();

  float current_multiplier_;
  float voltage_multiplier_;
  float power_multiplier_;
  float pulse_freq_;
  uint32_t expect_data_ts_;
  uint32_t prev_raw_current_{0};
  uint32_t prev_raw_energy_{0};
};

template<typename... Ts> class CS5460ARestartAction : public Action<Ts...> {
 public:
  CS5460ARestartAction(CS5460AComponent *cs5460a) : cs5460a_(cs5460a) {}

  void play(Ts... x) override { cs5460a_->restart(); }

 protected:
  CS5460AComponent *cs5460a_;
};

}  // namespace cs5460a
}  // namespace esphome
