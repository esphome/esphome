#include "mcp23016.h"
#include "esphome/core/log.h"
#include <cstdio>

namespace esphome::mcp23016 {

static const char *const TAG = "mcp23016";

void MCP23016::setup() {
  uint16_t iocon;
  // MCP23016 registers operate as paired 16-bit registers. Addressing the
  // odd register (e.g. IOCON1) reads/writes that register first, then wraps
  // to the even register (IOCON0) in the same pair. Starting from the odd
  // address gives the correct byte order for 1 << pin mapping:
  // high byte = port 1 (pins 8-15), low byte = port 0 (pins 0-7).
  if (!this->read_reg_(MCP23016_IOCON1, &iocon)) {
    this->mark_failed();
    return;
  }

  // Read current output register state
  this->read_reg_(MCP23016_OLAT1, &this->olat_);

  // all pins input
  this->write_reg_(MCP23016_IODIR1, 0xFFFF);

  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->setup();
    this->interrupt_pin_->attach_interrupt(&MCP23016::gpio_intr, this, gpio::INTERRUPT_FALLING_EDGE);
    this->set_invalidate_on_read_(false);
  }
  this->disable_loop();
}

void IRAM_ATTR MCP23016::gpio_intr(MCP23016 *arg) { arg->enable_loop_soon_any_context(); }
void MCP23016::loop() {
  // Invalidate cache at the start of each loop
  this->reset_pin_cache_();
  // Only disable the loop once INT has actually gone HIGH. Input transitions that straddle the
  // I2C read leave INT asserted without re-firing a falling edge, which would strand us with
  // stale state forever; keep looping until the line is released so we self-heal.
  if (this->interrupt_pin_ != nullptr && this->interrupt_pin_->digital_read()) {
    this->disable_loop();
  }
}
bool MCP23016::digital_read_hw(uint8_t pin) { return this->read_reg_(MCP23016_GP1, &this->input_mask_); }

bool MCP23016::digital_read_cache(uint8_t pin) { return this->input_mask_ & (1 << pin); }
void MCP23016::digital_write_hw(uint8_t pin, bool value) { this->update_reg_(pin, value, MCP23016_OLAT1); }
void MCP23016::pin_mode(uint8_t pin, gpio::Flags flags) {
  if (flags == gpio::FLAG_INPUT) {
    this->update_reg_(pin, true, MCP23016_IODIR1);
    if (this->interrupt_pin_ == nullptr) {
      this->enable_loop();
    }
  } else if (flags == gpio::FLAG_OUTPUT) {
    this->update_reg_(pin, false, MCP23016_IODIR1);
  }
}
float MCP23016::get_setup_priority() const { return setup_priority::IO; }
bool MCP23016::read_reg_(uint8_t reg, uint16_t *value) {
  if (this->is_failed())
    return false;

  return this->read_byte_16(reg, value);
}
bool MCP23016::write_reg_(uint8_t reg, uint16_t value) {
  if (this->is_failed())
    return false;

  return this->write_byte_16(reg, value);
}
void MCP23016::update_reg_(uint8_t pin, bool pin_value, uint8_t reg_addr) {
  uint16_t reg_value = 0;

  if (reg_addr == MCP23016_OLAT1) {
    reg_value = this->olat_;
  } else {
    this->read_reg_(reg_addr, &reg_value);
  }

  if (pin_value) {
    reg_value |= 1 << pin;
  } else {
    reg_value &= ~(1 << pin);
  }

  this->write_reg_(reg_addr, reg_value);

  if (reg_addr == MCP23016_OLAT1) {
    this->olat_ = reg_value;
  }
}

void MCP23016GPIOPin::setup() { pin_mode(flags_); }
void MCP23016GPIOPin::pin_mode(gpio::Flags flags) { this->parent_->pin_mode(this->pin_, flags); }
bool MCP23016GPIOPin::digital_read() { return this->parent_->digital_read(this->pin_) != this->inverted_; }
void MCP23016GPIOPin::digital_write(bool value) { this->parent_->digital_write(this->pin_, value != this->inverted_); }
size_t MCP23016GPIOPin::dump_summary(char *buffer, size_t len) const {
  return buf_append_printf(buffer, len, 0, "%u via MCP23016", this->pin_);
}

}  // namespace esphome::mcp23016
