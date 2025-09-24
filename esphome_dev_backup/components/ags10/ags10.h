#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace ags10 {

class AGS10Component : public PollingComponent, public i2c::I2CDevice {
 public:
  /**
   * Sets TVOC sensor.
   */
  void set_tvoc(sensor::Sensor *tvoc) { this->tvoc_ = tvoc; }

  /**
   * Sets version info sensor.
   */
  void set_version(sensor::Sensor *version) { this->version_ = version; }

  /**
   * Sets resistance info sensor.
   */
  void set_resistance(sensor::Sensor *resistance) { this->resistance_ = resistance; }

  void setup() override;

  void update() override;

  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::DATA; }

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
   * Sets zero-point with the value.
   */
  bool set_zero_point_with(uint16_t value);

 protected:
  /**
   *  TVOC.
   */
  sensor::Sensor *tvoc_{nullptr};

  /**
   * Firmvare version.
   */
  sensor::Sensor *version_{nullptr};

  /**
   * Resistance.
   */
  sensor::Sensor *resistance_{nullptr};

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

  /**
   * Calculates CRC8 value.
   *
   * CRC8 calculation, initial value: 0xFF, polynomial: 0x31 (x8+ x5+ x4+1)
   *
   * @param[in] dat the data buffer
   * @param num number of bytes in the buffer
   */
  template<size_t N> uint8_t calc_crc8_(std::array<uint8_t, N> dat, uint8_t num);
};

template<typename... Ts> class AGS10NewI2cAddressAction : public Action<Ts...>, public Parented<AGS10Component> {
 public:
  TEMPLATABLE_VALUE(uint8_t, new_address)

  void play(Ts... x) override { this->parent_->new_i2c_address(this->new_address_.value(x...)); }
};

enum AGS10SetZeroPointActionMode {
  // Zero-point reset.
  FACTORY_DEFAULT,
  // Zero-point calibration with current resistance.
  CURRENT_VALUE,
  // Zero-point calibration with custom resistance.
  CUSTOM_VALUE,
};

template<typename... Ts> class AGS10SetZeroPointAction : public Action<Ts...>, public Parented<AGS10Component> {
 public:
  TEMPLATABLE_VALUE(uint16_t, value)
  TEMPLATABLE_VALUE(AGS10SetZeroPointActionMode, mode)

  void play(Ts... x) override {
    switch (this->mode_.value(x...)) {
      case FACTORY_DEFAULT:
        this->parent_->set_zero_point_with_factory_defaults();
        break;
      case CURRENT_VALUE:
        this->parent_->set_zero_point_with_current_resistance();
        break;
      case CUSTOM_VALUE:
        this->parent_->set_zero_point_with(this->value_.value(x...));
        break;
    }
  }
};
}  // namespace ags10
}  // namespace esphome
