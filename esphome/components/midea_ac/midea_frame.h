#pragma once
#include "esphome/components/climate/climate.h"
#include "esphome/components/midea_dongle/midea_frame.h"

namespace esphome {
namespace midea_ac {

/// Enum for all modes a Midea device can be in.
enum MideaMode : uint8_t {
  /// The Midea device is set to automatically change the heating/cooling cycle
  MIDEA_MODE_AUTO = 1,
  /// The Midea device is manually set to cool mode (not in auto mode!)
  MIDEA_MODE_COOL = 2,
  /// The Midea device is manually set to dry mode
  MIDEA_MODE_DRY = 3,
  /// The Midea device is manually set to heat mode (not in auto mode!)
  MIDEA_MODE_HEAT = 4,
  /// The Midea device is manually set to fan only mode
  MIDEA_MODE_FAN_ONLY = 5,
};

/// Enum for all modes a Midea fan can be in
enum MideaFanMode : uint8_t {
  /// The fan mode is set to Auto
  MIDEA_FAN_AUTO = 102,
  /// The fan mode is set to Low
  MIDEA_FAN_LOW = 40,
  /// The fan mode is set to Medium
  MIDEA_FAN_MEDIUM = 60,
  /// The fan mode is set to High
  MIDEA_FAN_HIGH = 80,
};

/// Enum for all modes a Midea swing can be in
enum MideaSwingMode : uint8_t {
  /// The sing mode is set to Off
  MIDEA_SWING_OFF = 0b0000,
  /// The fan mode is set to Both
  MIDEA_SWING_BOTH = 0b1111,
  /// The fan mode is set to Vertical
  MIDEA_SWING_VERTICAL = 0b1100,
  /// The fan mode is set to Horizontal
  MIDEA_SWING_HORIZONTAL = 0b0011,
};

class PropertiesFrame : public midea_dongle::BaseFrame {
 public:
  PropertiesFrame() = delete;
  PropertiesFrame(uint8_t *data) : BaseFrame(data) {}
  PropertiesFrame(const Frame &frame) : BaseFrame(frame) {}

  template<typename TF> typename std::enable_if<std::is_base_of<PropertiesFrame, TF>::value, bool>::type is() const {
    return (this->resp_type_() == 0xC0) && (this->get_type() == 0x03 || this->get_type() == 0x02);
  }

  /* TARGET TEMPERATURE */

  float get_target_temp() const;
  void set_target_temp(float temp);

  /* MODE */
  climate::ClimateMode get_mode() const;
  void set_mode(climate::ClimateMode mode);

  /* FAN SPEED */
  climate::ClimateFanMode get_fan_mode() const;
  void set_fan_mode(climate::ClimateFanMode mode);

  /* SWING MODE */
  climate::ClimateSwingMode get_swing_mode() const;
  void set_swing_mode(climate::ClimateSwingMode mode);

  /* INDOOR TEMPERATURE */
  float get_indoor_temp() const;

  /* OUTDOOR TEMPERATURE */
  float get_outdoor_temp() const;

  /* ECO MODE */
  bool get_eco_mode() const { return this->pbuf_[19]; }
  void set_eco_mode(bool state) { this->set_bytemask_(19, 0xFF, state); }

  /* SLEEP MODE */
  bool get_sleep_mode() const { return this->pbuf_[20] & 0x01; }
  void set_sleep_mode(bool state) { this->set_bytemask_(20, 0x01, state); }

  /* TURBO MODE */
  bool get_turbo_mode() const { return this->pbuf_[20] & 0x02; }
  void set_turbo_mode(bool state) { this->set_bytemask_(20, 0x02, state); }

  /// Set properties from another frame
  void set_properties(const PropertiesFrame &p) { memcpy(this->pbuf_ + 11, p.data() + 11, 10); }

 protected:
  /* POWER */
  bool get_power_() const { return this->pbuf_[11] & 0x01; }
  void set_power_(bool state) { this->set_bytemask_(11, 0x01, state); }
};

// Query state frame (read-only)
class QueryFrame : public midea_dongle::StaticFrame<midea_dongle::Frame> {
 public:
  QueryFrame() : StaticFrame(FPSTR(this->INIT)) {}

 private:
  static const uint8_t PROGMEM INIT[];
};

// Query state frame (read-only)
class CommandFrame : public midea_dongle::StaticFrame<PropertiesFrame> {
 public:
  CommandFrame() : StaticFrame(FPSTR(this->INIT)) {}
  void set_beeper_feedback(bool state) { this->set_bytemask_(11, 0x40, state); }

 private:
  static const uint8_t PROGMEM INIT[];
};

}  // namespace midea_ac
}  // namespace esphome
