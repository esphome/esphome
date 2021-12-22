#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace bh1750 {

/// Enum listing all resolutions that can be used with the BH1750
enum BH1750Resolution {
  BH1750_RESOLUTION_4P0_LX = 0b00100011,  // one-time low resolution mode
  BH1750_RESOLUTION_1P0_LX = 0b00100000,  // one-time high resolution mode 1
  BH1750_RESOLUTION_0P5_LX = 0b00100001,  // one-time high resolution mode 2
};

/// This class implements support for the i2c-based BH1750 ambient light sensor.
class BH1750Sensor : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  /** Set the resolution of this sensor.
   *
   * Possible values are:
   *
   *  - `BH1750_RESOLUTION_4P0_LX`
   *  - `BH1750_RESOLUTION_1P0_LX`
   *  - `BH1750_RESOLUTION_0P5_LX` (default)
   *
   * @param resolution The new resolution of the sensor.
   */
  void set_resolution(BH1750Resolution resolution);
  void set_measurement_duration(uint8_t measurement_duration) { measurement_duration_ = measurement_duration; }

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  void setup() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override;

 protected:
  void read_data_();

  BH1750Resolution resolution_{BH1750_RESOLUTION_0P5_LX};
  uint8_t measurement_duration_;
};

}  // namespace bh1750
}  // namespace esphome
