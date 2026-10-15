#pragma once

#include <array>
#include <cstdint>

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::as734x {

enum Gain : uint8_t {
  GAIN_0_5X,
  GAIN_1X,
  GAIN_2X,
  GAIN_4X,
  GAIN_8X,
  GAIN_16X,
  GAIN_32X,
  GAIN_64X,
  GAIN_128X,
  GAIN_256X,
  GAIN_512X,
  GAIN_1024X,  // AS7343 only
  GAIN_2048X,  // AS7343 only
};

static constexpr uint8_t NUM_BANDS = 13;  // band order of BANDS_43 in __init__.py

struct RegValue {
  uint8_t reg;
  uint8_t value;
};

/// Per-chip constants, stored in flash. The pointed-to tables are in flash too.
struct ChipInfo {
  const char *name;
  const RegValue *init;    // written in order; the bank is selected per register address
  const uint8_t *smux;     // AS7341: SMUX RAM image per step; AS7343: the three chain images
  const uint8_t *readout;  // per step, the ADC slot of each band (0xFF: not read in this step)
  uint8_t init_count;
  uint8_t astep_reg;
  uint8_t cfg0_reg;
  uint8_t cfg1_reg;
  uint8_t status2_reg;
  uint8_t id_reg;
  uint8_t id_mask;
  uint8_t id_value;
  uint8_t adc_slots;   // 16-bit data registers read after ASTATUS
  uint8_t smux_steps;  // measurements per update
  uint8_t integration_cycles;
  uint8_t smux_len;          // bytes per SMUX image
  bool auto_smux;            // the chip cycles its SMUX itself after a one-time chain setup
  uint8_t extra_clear_slot;  // averaged into the clear band (0xFF: none)
};

#ifdef USE_AS7341
extern const ChipInfo AS7341_CHIP;
#endif
#ifdef USE_AS7343
extern const ChipInfo AS7343_CHIP;
extern const ChipInfo TCS3448_CHIP;
#endif

class AS734XComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  explicit AS734XComponent(const ChipInfo *chip) : chip_(chip) {}

  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;

  void set_gain(Gain gain) { this->gain_ = gain; }
  void set_atime(uint8_t atime) { this->atime_ = atime; }
  void set_astep(uint16_t astep) { this->astep_ = astep; }
  void set_counts_sensor(sensor::Sensor *sensor, uint8_t band) { this->sensors_[band] = sensor; }

 protected:
  bool select_bank_(uint8_t reg);
  bool start_step_();
  void read_step_();
  void abort_measurement_(const LogString *reason);

  std::array<sensor::Sensor *, NUM_BANDS> sensors_{};
  const ChipInfo *chip_;  // in flash, read field by field with progmem_memcpy
  uint32_t start_ms_{0};
  uint32_t integration_ms_{0};
  uint32_t timeout_ms_{0};
  uint16_t astep_{0};
  enum class State : uint8_t { STATE_IDLE, STATE_WAIT_SMUX, STATE_READ_DATA } state_{State::STATE_IDLE};
  Gain gain_{GAIN_1X};
  uint8_t atime_{0};
  uint8_t step_{0};
  bool bank_low_{true};  // unknown until setup() writes CFG0
};

}  // namespace esphome::as734x
