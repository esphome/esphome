#pragma once
#include "esphome/components/climate/climate.h"
#include "esphome/components/midea_dongle/midea_frame.h"

namespace esphome {
namespace midea_ac {

class PropertiesFrame : public midea_dongle::BaseFrame {
 public:
  PropertiesFrame() = delete;
  PropertiesFrame(uint8_t *data) : BaseFrame(data) {}
  PropertiesFrame(const Frame &frame) : BaseFrame(frame) {}

  template <typename TF>
  typename std::enable_if<std::is_base_of<PropertiesFrame, TF>::value,
    bool>::type is() const {
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
  
  void disable_timer_on() {
    this->pbuf_[14] = 0x7F;
    this->pbuf_[16] &= ~0xF0;
  }
  
  void disable_timer_off() {
    this->pbuf_[15] = 0x7F;
    this->pbuf_[16] &= ~0x0F;
  }
  
  /* SWING MODE */
  climate::ClimateSwingMode get_swing_mode() const;
  void set_swing_mode(climate::ClimateSwingMode mode);

  /* INDOOR TEMPERATURE */
  float get_indoor_temp() const;
    
  /* OUTDOOR TEMPERATURE */
  float get_outdoor_temp() const;

 protected:
    /* POWER */
    bool get_power_() const {
        return this->pbuf_[11] & 0x01;
    }
    void set_power_(bool state) {
        this->set_bytemask_(11, 0x01, state);
    }
};

// Query state frame (read-only)
class QueryFrame : public midea_dongle::StaticFrame<midea_dongle::Frame>
{
 public:
  QueryFrame() : StaticFrame(FPSTR(this->INIT_)) {}
 private:
  static const uint8_t PROGMEM INIT_[];
};

// Query state frame (read-only)
class CommandFrame : public midea_dongle::StaticFrame<PropertiesFrame> {
 public:
  CommandFrame() : StaticFrame(FPSTR(this->INIT_)) {}
  void set_beeper_feedback(bool state) {
    this->set_bytemask_(11, 0x40, state);
  }
 private:
  static const uint8_t PROGMEM INIT_[];
};

} // namespace midea_ac
} // namespace esphome
