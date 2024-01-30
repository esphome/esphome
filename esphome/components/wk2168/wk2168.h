/// @file wk2168.h
/// @author DrCoolZic
/// @brief  wk2168 classes declaration

#pragma once
#include "esphome/components/wk_base/wk_base.h"

namespace esphome {
namespace wk2168 {

////////////////////////////////////////////////////////////////////////////////////
/// @brief The WK2168Component class stores the information global to the WK2168 component
/// and provides methods to set/access this information.
////////////////////////////////////////////////////////////////////////////////////
class WK2168Component : public wk_base::WKBaseComponent {
 public:
  void loop() override;

 protected:
  friend class WK2168GPIOPin;
#ifdef TEST_COMPONENT
  void test_gpio_input_();
  void test_gpio_output_();
#endif

  /// Helper method to read the value of a pin.
  bool read_pin_val_(uint8_t pin);

  /// Helper method to write the value of a pin.
  void write_pin_val_(uint8_t pin, bool value);

  /// Helper method to set the pin mode of a pin.
  void set_pin_direction_(uint8_t pin, gpio::Flags flags);

  uint8_t pin_config_{0x00};    ///< pin config mask: 1 means OUTPUT, 0 means INPUT
  uint8_t output_state_{0x00};  ///< output state: 1 means HIGH, 0 means LOW
  uint8_t input_state_{0x00};   ///< input pin states: 1 means HIGH, 0 means LOW
};

///////////////////////////////////////////////////////////////////////////////
/// @brief Helper class to expose a WK2168 pin as an internal input GPIO pin.
///////////////////////////////////////////////////////////////////////////////
class WK2168GPIOPin : public GPIOPin {
 public:
  void set_parent(WK2168Component *parent) { this->parent_ = parent; }
  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void set_inverted(bool inverted) { this->inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { this->flags_ = flags; }

  void setup() override;
  std::string dump_summary() const override;

  void pin_mode(gpio::Flags flags) override { this->parent_->set_pin_direction_(this->pin_, flags); }
  bool digital_read() override { return this->parent_->read_pin_val_(this->pin_) != this->inverted_; }
  void digital_write(bool value) override { this->parent_->write_pin_val_(this->pin_, value != this->inverted_); }

 protected:
  WK2168Component *parent_{nullptr};
  uint8_t pin_;
  bool inverted_;
  gpio::Flags flags_;
};

}  // namespace wk2168
}  // namespace esphome
