#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace htu31d {

/** Default I2C address for the HTU31D. */
#define HTU31D_DEFAULT_I2CADDR (0x40)

/** Read temperature and humidity. */
#define HTU31D_READTEMPHUM (0x00)

/** Start a conversion! */
#define HTU31D_CONVERSION (0x40)

/** Read serial number command. */
#define HTU31D_READSERIAL (0x0A)

/** Enable heater */
#define HTU31D_HEATERON (0x04)

/** Disable heater */
#define HTU31D_HEATEROFF (0x02)


/** Reset command. */
#define HTU31D_RESET (0x1E)

/** Diagnostics command. */
#define HTU31D_DIAGNOSTICS (0x08)

class HTU31DComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;        /// Setup (reset) the sensor and check connection.
  void update() override;       /// Update the sensor values (temperature+humidity).
  void dump_config() override;  /// Dumps the configuration values.

  void set_temperature(sensor::Sensor *temperature) { temperature_ = temperature; }
  void set_humidity(sensor::Sensor *humidity) { humidity_ = humidity; }
  void set_heater(sensor::Sensor *heater) { heater_ = heater; }

  void set_heater_state(bool desired);
  bool is_heater_enabled();

  float get_setup_priority() const override;

 private:
  bool reset_();
  uint32_t read_serial_num_();
  uint8_t compute_crc_(uint32_t value);

 protected:
  sensor::Sensor *temperature_{nullptr};
  sensor::Sensor *humidity_{nullptr};
  sensor::Sensor *heater_{nullptr};
};
}  // namespace htu31d
}  // namespace esphome
