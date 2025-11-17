#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::bh1750 {

enum BH1750Mode : uint8_t {
  BH1750_MODE_L,
  BH1750_MODE_H,
  BH1750_MODE_H2,
};

/// This class implements support for the i2c-based BH1750 ambient light sensor.
class BH1750Sensor : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;
  float get_setup_priority() const override;

 protected:
  // State machine states
  enum State : uint8_t {
    IDLE,
    WAITING_COARSE_MEASUREMENT,
    READING_COARSE_RESULT,
    WAITING_FINE_MEASUREMENT,
    READING_FINE_RESULT,
  };

  // 4-byte aligned members
  uint32_t measurement_start_time_{0};
  uint32_t measurement_duration_{0};

  // 1-byte members grouped together to minimize padding
  State state_{IDLE};
  BH1750Mode current_mode_{BH1750_MODE_L};
  uint8_t current_mtreg_{31};
  BH1750Mode fine_mode_{BH1750_MODE_H2};
  uint8_t fine_mtreg_{254};
  uint8_t active_mtreg_{0};

  // Helper methods
  bool start_measurement_(BH1750Mode mode, uint8_t mtreg, uint32_t now);
  bool read_measurement_(float &lx_out);
  void process_coarse_result_(float lx);
  void fail_and_reset_();
};

}  // namespace esphome::bh1750
