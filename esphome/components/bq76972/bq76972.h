#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

#include <array>

namespace esphome::bq76972 {

static constexpr size_t BQ76972_NUM_CELLS = 16;
static constexpr size_t BQ76972_NUM_TEMPERATURES = 10;

class BQ76972Component final : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  void set_crc_mode(bool crc_mode) { this->crc_mode_ = crc_mode; }
  void set_reg_disable(bool reg_disable) { this->reg_disable_ = reg_disable; }
#ifdef USE_SENSOR
  void set_stack_voltage_sensor(sensor::Sensor *s) { this->stack_voltage_sensor_ = s; }
  void set_cell_sensor(sensor::Sensor *s, size_t index) { this->cell_sensors_[index] = s; }
  void set_temperature_sensor(sensor::Sensor *s, size_t index) { this->temperature_sensors_[index] = s; }
#endif

  void program_i2c_address(uint8_t address);

 protected:
  bool read_block_(uint8_t start_register, uint8_t *data, size_t len);
  bool write_block_(uint8_t start_register, const uint8_t *data, size_t len);
  bool read_words_(uint8_t start_register, uint16_t *data, size_t num_words);
  bool send_subcommand_(uint16_t subcommand);
  bool read_subcommand_(uint16_t subcommand, uint8_t *data, size_t len);
  bool write_subcommand_(uint16_t subcommand, const uint8_t *data, size_t len);
  bool wait_for_subcommand_();
  bool wait_for_cfgupdate_();
  bool enter_cfg_update_();
  bool exit_cfg_update_();
  bool regs_disabled_(bool &out);
  const LogString *configure_();
  const LogString *write_config_(bool write_regs);

  bool reg_disable_{false};
  bool crc_mode_{false};

#ifdef USE_SENSOR
  sensor::Sensor *stack_voltage_sensor_{nullptr};
  std::array<sensor::Sensor *, BQ76972_NUM_CELLS> cell_sensors_{};
  std::array<sensor::Sensor *, BQ76972_NUM_TEMPERATURES> temperature_sensors_{};
#endif
};

}  // namespace esphome::bq76972
