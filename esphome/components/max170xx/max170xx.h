#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::max170xx {

// Shared base for the ModelGauge fuel gauges.
// MAX17043: https://www.analog.com/en/products/max17043.html
// MAX17048: https://www.analog.com/en/products/max17048.html
// The MAX17048 is register-compatible with the MAX17043, and adds a STATUS register and a MODE.EnSleep gate in
// front of CONFIG.SLEEP.

static constexpr uint8_t REG_VCELL = 0x02;
static constexpr uint8_t REG_SOC = 0x04;
static constexpr uint8_t REG_MODE = 0x06;
static constexpr uint8_t REG_VERSION = 0x08;
static constexpr uint8_t REG_CONFIG = 0x0C;
static constexpr uint8_t REG_STATUS = 0x1A;

static constexpr uint16_t CONFIG_POWER_UP_DEFAULT = 0x971C;
// Mask out SLEEP (7), ALRT (5), and bit 6, which is a don't-care on the MAX17043 but ALSC on the MAX17048.
static constexpr uint16_t CONFIG_SAFE_MASK = 0xFF1F;
static constexpr uint16_t CONFIG_SLEEP = 0x0080;
static constexpr uint16_t CONFIG_ALRT = 0x0020;

// CONFIG.SLEEP is ignored unless MODE.EnSleep (bit 13) is set first.
static constexpr uint16_t MODE_EN_SLEEP = 0x2000;
static constexpr uint16_t STATUS_HD = 0x1000;

static constexpr uint32_t WAKE_ALERT_CLEAR_TIMEOUT_ID = 0;
// The gauge raises its false wake alert some time after the wake write; on hardware it rose 0.8 to 1.0 s later
static constexpr uint32_t WAKE_ALERT_CLEAR_DELAY_MS = 2000;

enum class Model : uint8_t {
  MODEL_MAX17043,
  MODEL_MAX17048,
};

/// Defined in max170xx.cpp, which instantiates both models.
template<Model MODEL> class MAX170xxComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void sleep_mode();

 protected:
  /// True once the gauge is configured; otherwise retries the configuration and warns if the gauge did not answer.
  /// Call first in update().
  bool ready_for_update_();
  /// Verify and wake the gauge; false if it did not answer.
  bool configure_();
  /// Clear the false alert raised on waking, leaving every other CONFIG and STATUS bit as it is.
  void clear_wake_alert_();

  bool configured_{false};
};

extern template class MAX170xxComponent<Model::MODEL_MAX17043>;
extern template class MAX170xxComponent<Model::MODEL_MAX17048>;

}  // namespace esphome::max170xx
