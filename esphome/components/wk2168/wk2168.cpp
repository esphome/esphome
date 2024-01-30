/// @file wk2168.cpp
/// @author DrCoolzic
/// @brief wk2168 classes implementation

#include "wk2168.h"

namespace esphome {
namespace wk2168 {

using namespace wk_base;

static const char *const TAG = "wk2168";

/// @brief convert an int to binary representation as C++ std::string
/// @param val integer to convert
/// @return a std::string
inline std::string i2s(uint8_t val) { return std::bitset<8>(val).to_string(); }
/// Convert std::string to C string
#define S2CS(val) (i2s(val).c_str())

/// @brief measure the time elapsed between two calls
/// @param last_time time of the previous call
/// @return the elapsed time in microseconds
uint32_t elapsed(uint32_t &last_time) {
  uint32_t e = millis() - last_time;
  last_time = millis();
  return e;
};

///////////////////////////////////////////////////////////////////////////////
// The WK2168Component methods
///////////////////////////////////////////////////////////////////////////////
#ifdef TEST_COMPONENT
void WK2168Component::test_gpio_input_() {
  static bool init_input{false};
  static uint8_t state{0};
  uint8_t value;
  if (!init_input) {
    init_input = true;
    // set all pins in input mode
    this->reg(WKREG_GPDIR, 0) = 0x00;
    ESP_LOGI(TAG, "initializing all pins to input mode");
    state = this->reg(WKREG_GPDAT, 0);
    ESP_LOGI(TAG, "initial input state = %02X (%s)", state, S2CS(state));
  }
  value = this->reg(WKREG_GPDAT, 0);
  if (value != state) {
    ESP_LOGI(TAG, "Input value changed from %02X to %02X (%s)", state, value, S2CS(value));
    state = value;
  }
}

void WK2168Component::test_gpio_output_() {
  static bool init_output{false};
  static uint8_t state{0};
  if (!init_output) {
    init_output = true;
    // set all pins in output mode
    this->reg(WKREG_GPDIR, 0) = 0x00;
    ESP_LOGI(TAG, "initializing all pins to output mode");
    this->reg(WKREG_GPDAT, 0) = state;
    ESP_LOGI(TAG, "setting all outputs to 0");
  }
  state = ~state;
  this->reg(WKREG_GPDAT, 0) = state;
  ESP_LOGI(TAG, "Flipping all outputs to %02X (%s)", state, S2CS(state));
}
#endif

void WK2168Component::loop() {
  wk_base::WKBaseComponent::loop();  // we call the loop() method from base class
  if ((this->component_state_ & COMPONENT_STATE_MASK != COMPONENT_STATE_LOOP) && !test_mode_)
    return;

#ifdef TEST_COMPONENT
  static uint32_t loop_time = 0;
  static uint32_t loop_count = 0;
  uint32_t time = 0;
  ESP_LOGV(TAG, "Component wk2168 loop %d for %s : %d ms since last call ...", loop_count, this->get_name(),
           millis() - loop_time);
  loop_time = millis();
  loop_count++;

  if (test_mode_ == 2) {
    elapsed(time);  // set time to now
    test_gpio_input_();
    ESP_LOGV(TAG, "test gpio input - execution time %d ms...", elapsed(time));
  }

  if (test_mode_ == 3) {
    elapsed(time);  // set time to now
    test_gpio_output_();
    ESP_LOGV(TAG, "test gpio output - execution time %d ms...", elapsed(time));
  }
  ESP_LOGV(TAG, "loop execution time %d ms...", millis() - loop_time);

#endif
}

bool WK2168Component::read_pin_val_(uint8_t pin) {
  this->input_state_ = this->reg(WKREG_GPDAT, 0);
  ESP_LOGVV(TAG, "reading input pin %d = %d in_state %s", pin, this->input_state_ & (1 << pin), S2CS(input_state_));
  return this->input_state_ & (1 << pin);
}

void WK2168Component::write_pin_val_(uint8_t pin, bool value) {
  value ? this->output_state_ |= (1 << pin) : this->output_state_ &= ~(1 << pin);
  ESP_LOGVV(TAG, "writing output pin %d with %d out_state %s", pin, value, S2CS(this->output_state_));
  this->reg(WKREG_GPDAT, 0) = this->output_state_;
}

void WK2168Component::set_pin_direction_(uint8_t pin, gpio::Flags flags) {
  if (flags == gpio::FLAG_INPUT) {
    this->pin_config_ &= ~(1 << pin);  // clear bit (input mode)
  } else {
    if (flags == gpio::FLAG_OUTPUT) {
      this->pin_config_ |= 1 << pin;  // set bit (output mode)
    } else {
      ESP_LOGE(TAG, "pin %d direction invalid", pin);
    }
  }
  ESP_LOGVV(TAG, "setting pin %d direction to %d pin_config=%s", pin, flags, S2CS(this->pin_config_));
  this->reg(WKREG_GPDIR, 0) = this->pin_config_;  // TODO check ~
}

///////////////////////////////////////////////////////////////////////////////
// The WK2168GPIOPin methods
///////////////////////////////////////////////////////////////////////////////

void WK2168GPIOPin::setup() {
  ESP_LOGV(TAG, "Setting GPIO pin %d mode to %s", this->pin_,
           flags_ == gpio::FLAG_INPUT          ? "Input"
           : this->flags_ == gpio::FLAG_OUTPUT ? "Output"
                                               : "NOT SPECIFIED");
  // ESP_LOGCONFIG(TAG, "Setting GPIO pins direction/mode to '%s' %02X", i2s_(flags_), flags_);
  this->pin_mode(this->flags_);
}

std::string WK2168GPIOPin::dump_summary() const {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%u via WK2168 %s", this->pin_, this->parent_->get_name());
  return buffer;
}

}  // namespace wk2168
}  // namespace esphome
