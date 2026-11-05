#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome::ags10 {

enum AGS10SetZeroPointActionMode {
  // Zero-point reset.
  FACTORY_DEFAULT,
  // Zero-point calibration with current resistance.
  CURRENT_VALUE,
  // Zero-point calibration with custom resistance.
  CUSTOM_VALUE,
};

class AGS10Component final : public PollingComponent, public i2c::I2CDevice {
 public:
  /**
   * Sets TVOC sensor.
   */
  SUB_SENSOR(tvoc)

  /**
   * Sets version info sensor.
   */
  SUB_SENSOR(version)

  /**
   * Sets resistance info sensor.
   */
  SUB_SENSOR(resistance)

  void setup() override;

  void update() override;

  void dump_config() override;

  /**
   * Modifies target address of AGS10.
   *
   * New address is saved and takes effect immediately even after power-off.
   */
  bool new_i2c_address(uint8_t newaddress);

  /**
   * Sets zero-point with factory defaults.
   */
  bool set_zero_point_with_factory_defaults();

  /**
   * Sets zero-point with current sensor resistance.
   */
  bool set_zero_point_with_current_resistance();

  /**
   * Sets zero-point by mode; the value is only used for CUSTOM_VALUE.
   */
  void set_zero_point(AGS10SetZeroPointActionMode mode, uint16_t value);

  /**
   * Sets zero-point with the value.
   */
  bool set_zero_point_with(uint16_t value);

 protected:
  /**
   *  Last operation error code.
   */
  enum ErrorCode {
    NONE = 0,
    COMMUNICATION_FAILED,
    CRC_CHECK_FAILED,
    ILLEGAL_STATUS,
    UNSUPPORTED_UNITS,
  } error_code_{NONE};

  /**
   *  Reads and returns value of TVOC.
   */
  optional<uint32_t> read_tvoc_();

  /**
   *  Reads and returns a firmware version of AGS10.
   */
  optional<uint8_t> read_version_();

  /**
   * Reads and returns the resistance of AGS10.
   */
  optional<uint32_t> read_resistance_();

  /**
   * Read, checks and returns data from the sensor.
   */
  template<size_t N> optional<std::array<uint8_t, N>> read_and_check_(uint8_t a_register);
};

}  // namespace esphome::ags10
