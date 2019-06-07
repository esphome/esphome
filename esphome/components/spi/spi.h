#pragma once

#include "esphome/core/component.h"
#include "esphome/core/esphal.h"
#include "esphome/core/application.h"

namespace esphome {
namespace spi {

enum SPIBitOrder {
  BIT_ORDER_LSB_FIRST,
  BIT_ORDER_MSB_FIRST,
};
enum SPIClockPolarity {
  CLOCK_POLARITY_LOW = false,
  CLOCK_POLARITY_HIGH = true,
};
enum SPIClockPhase {
  CLOCK_PHASE_LEADING,
  CLOCK_PHASE_TRAILING,
};
enum SPIDataRate : uint32_t {
  DATA_RATE_1KHZ = 1000,
  DATA_RATE_1MHZ = 1000000,
  DATA_RATE_2MHZ = 2000000,
  DATA_RATE_4MHZ = 4000000,
  DATA_RATE_8MHZ = 8000000,
};

class SPIComponent : public Component {
 public:
  void set_clk(GPIOPin *clk) { clk_ = clk; }
  void set_miso(GPIOPin *miso) { miso_ = miso; }
  void set_mosi(GPIOPin *mosi) { mosi_ = mosi; }

  void setup() override;

  void dump_config() override;

  template<SPIBitOrder BIT_ORDER, SPIClockPolarity CLOCK_POLARITY, SPIClockPhase CLOCK_PHASE> uint8_t read_byte() {
    return this->transfer_<BIT_ORDER, CLOCK_POLARITY, CLOCK_PHASE, true, false>(0x00);
  }

  template<SPIBitOrder BIT_ORDER, SPIClockPolarity CLOCK_POLARITY, SPIClockPhase CLOCK_PHASE>
  void read_array(uint8_t *data, size_t length) {
    for (size_t i = 0; i < length; i++) {
      App.feed_wdt();
      data[i] = this->read_byte<BIT_ORDER, CLOCK_POLARITY, CLOCK_PHASE>();
    }
  }

  template<SPIBitOrder BIT_ORDER, SPIClockPolarity CLOCK_POLARITY, SPIClockPhase CLOCK_PHASE>
  void write_byte(uint8_t data) {
    this->transfer_<BIT_ORDER, CLOCK_POLARITY, CLOCK_PHASE, true, false>(data);
  }

  template<SPIBitOrder BIT_ORDER, SPIClockPolarity CLOCK_POLARITY, SPIClockPhase CLOCK_PHASE>
  void write_array(uint8_t *data, size_t length) {
    for (size_t i = 0; i < length; i++) {
      App.feed_wdt();
      this->write_byte<BIT_ORDER, CLOCK_POLARITY, CLOCK_PHASE>(data[i]);
    }
  }

  template<SPIBitOrder BIT_ORDER, SPIClockPolarity CLOCK_POLARITY, SPIClockPhase CLOCK_PHASE>
  uint8_t transfer_byte(uint8_t data) {
    return this->transfer_<BIT_ORDER, CLOCK_POLARITY, CLOCK_PHASE, true, true>(data);
  }

  template<SPIBitOrder BIT_ORDER, SPIClockPolarity CLOCK_POLARITY, SPIClockPhase CLOCK_PHASE>
  void transfer_array(uint8_t *data, size_t length) {
    for (size_t i = 0; i < length; i++) {
      App.feed_wdt();
      data[i] = this->transfer_byte<BIT_ORDER, CLOCK_POLARITY, CLOCK_PHASE>(data[i]);
    }
  }

  void enable(GPIOPin *cs, uint32_t wait_cycle);

  void disable();

  float get_setup_priority() const override;

 protected:
  inline void cycle_clock_(bool value) {
    this->clk_->digital_write(value);
    const uint32_t start = ESP.getCycleCount();
    while (start - ESP.getCycleCount() < this->wait_cycle_)
      ;
  }

  static void debug_tx(uint8_t value);
  static void debug_rx(uint8_t value);

  template<SPIBitOrder BIT_ORDER, SPIClockPolarity CLOCK_POLARITY, SPIClockPhase CLOCK_PHASE, bool READ, bool WRITE>
  uint8_t transfer_(uint8_t data);

  GPIOPin *clk_;
  GPIOPin *miso_{nullptr};
  GPIOPin *mosi_{nullptr};
  GPIOPin *active_cs_{nullptr};
  uint32_t wait_cycle_;
};

template<SPIBitOrder BIT_ORDER, SPIClockPolarity CLOCK_POLARITY, SPIClockPhase CLOCK_PHASE, SPIDataRate DATA_RATE>
class SPIDevice {
 public:
  SPIDevice() = default;
  SPIDevice(SPIComponent *parent, GPIOPin *cs) : parent_(parent), cs_(cs) {}

  void set_spi_parent(SPIComponent *parent) { parent_ = parent; }
  void set_cs_pin(GPIOPin *cs) { cs_ = cs; }

  void spi_setup() {
    this->cs_->setup();
    this->cs_->digital_write(true);
  }

  void enable() { this->parent_->enable(this->cs_, DATA_RATE / F_CPU); }

  void disable() { this->parent_->disable(); }

  uint8_t read_byte() { return this->parent_->template read_byte<BIT_ORDER, CLOCK_POLARITY, CLOCK_PHASE>(); }

  void read_array(uint8_t *data, size_t length) {
    return this->parent_->template read_array<BIT_ORDER, CLOCK_POLARITY, CLOCK_PHASE>(data, length);
  }

  void write_byte(uint8_t data) {
    return this->parent_->template write_byte<BIT_ORDER, CLOCK_POLARITY, CLOCK_PHASE>(data);
  }

  void write_array(uint8_t *data, size_t length) {
    this->parent_->template write_array<BIT_ORDER, CLOCK_POLARITY, CLOCK_PHASE>(data, length);
  }

  uint8_t transfer_byte(uint8_t data) {
    return this->parent_->template transfer_byte<BIT_ORDER, CLOCK_POLARITY, CLOCK_PHASE>(data);
  }

  void transfer_array(uint8_t *data, size_t length) {
    this->parent_->template transfer_array<BIT_ORDER, CLOCK_POLARITY, CLOCK_PHASE>(data, length);
  }

 protected:
  SPIComponent *parent_{nullptr};
  GPIOPin *cs_{nullptr};
};

}  // namespace spi
}  // namespace esphome
