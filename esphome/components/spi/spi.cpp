#include "spi.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/application.h"

namespace esphome {
namespace spi {

static const char *TAG = "spi";

void ICACHE_RAM_ATTR HOT SPIComponent::enable(GPIOPin *cs, uint32_t wait_cycle) {
  ESP_LOGVV(TAG, "Enabling SPI Chip on pin %u...", cs->get_pin());
  this->wait_cycle_ = wait_cycle;
  this->active_cs_ = cs;
  this->active_cs_->digital_write(false);
}

void ICACHE_RAM_ATTR HOT SPIComponent::disable() {
  ESP_LOGVV(TAG, "Disabling SPI Chip on pin %u...", this->active_cs_->get_pin());
  this->active_cs_->digital_write(true);
  this->active_cs_ = nullptr;
}
void SPIComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SPI bus...");
  this->clk_->setup();
  this->clk_->digital_write(true);
  if (this->miso_ != nullptr) {
    this->miso_->setup();
  }
  if (this->mosi_ != nullptr) {
    this->mosi_->setup();
    this->mosi_->digital_write(false);
  }
}
void SPIComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SPI bus:");
  LOG_PIN("  CLK Pin: ", this->clk_);
  LOG_PIN("  MISO Pin: ", this->miso_);
  LOG_PIN("  MOSI Pin: ", this->mosi_);
}
float SPIComponent::get_setup_priority() const { return setup_priority::BUS; }

void SPIComponent::debug_tx_(uint8_t value) {
  ESP_LOGVV(TAG, "    TX 0b" BYTE_TO_BINARY_PATTERN " (0x%02X)", BYTE_TO_BINARY(value), value);
}
void SPIComponent::debug_rx_(uint8_t value) {
  ESP_LOGVV(TAG, "    RX 0b" BYTE_TO_BINARY_PATTERN " (0x%02X)", BYTE_TO_BINARY(value), value);
}

#pragma GCC optimize ("unroll-loops")
#pragma GCC optimize ("O2")

template<SPIBitOrder BIT_ORDER, SPIClockPolarity CLOCK_POLARITY, SPIClockPhase CLOCK_PHASE, bool READ, bool WRITE>
uint8_t HOT SPIComponent::transfer_(uint8_t data) {
  // Clock starts out at idle level
  this->clk_->digital_write(CLOCK_POLARITY);
  uint8_t out_data = 0;

  for (uint8_t i = 0; i < 8; i++) {
    uint8_t shift;
    if (BIT_ORDER == BIT_ORDER_MSB_FIRST)
      shift = 7 - i;
    else
      shift = i;

    if (CLOCK_PHASE == CLOCK_PHASE_LEADING) {
      // sampling on leading edge
      if (WRITE) {
        this->mosi_->digital_write(data & (1 << shift));
      }

      // SAMPLE!
      this->cycle_clock_(!CLOCK_POLARITY);

      if (READ) {
        out_data |= this->miso_->digital_read() << shift;
      }

      this->cycle_clock_(CLOCK_POLARITY);
    } else {
      // sampling on falling edge
      this->cycle_clock_(!CLOCK_POLARITY);

      if (WRITE) {
        this->mosi_->digital_write(data & (1 << shift));
      }

      // SAMPLE!
      this->cycle_clock_(CLOCK_POLARITY);

      if (READ) {
        out_data |= this->miso_->digital_read() << shift;
      }
    }
  }

#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
  if (WRITE) {
    SPIComponent::debug_tx_(data);
  }
  if (READ) {
    SPIComponent::debug_rx_(out_data);
  }
#endif

  return out_data;
}

// Generate with (py3):
//
// from itertools import product
// bit_orders = ['BIT_ORDER_LSB_FIRST', 'BIT_ORDER_MSB_FIRST']
// clock_pols = ['CLOCK_POLARITY_LOW', 'CLOCK_POLARITY_HIGH']
// clock_phases = ['CLOCK_PHASE_LEADING', 'CLOCK_PHASE_TRAILING']
// reads = [False, True]
// writes = [False, True]
// cpp_bool = {False: 'false', True: 'true'}
// for b, cpol, cph, r, w in product(bit_orders, clock_pols, clock_phases, reads, writes):
//     if not r and not w:
//         continue
//     print(f"template uint8_t ICACHE_RAM_ATTR SPIComponent::transfer_<{b}, {cpol}, {cph}, {cpp_bool[r]}, {cpp_bool[w]}>(uint8_t data);")

template uint8_t SPIComponent::transfer_<BIT_ORDER_LSB_FIRST, CLOCK_POLARITY_LOW, CLOCK_PHASE_LEADING, false, true>(uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_LSB_FIRST, CLOCK_POLARITY_LOW, CLOCK_PHASE_LEADING, true, false>(uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_LSB_FIRST, CLOCK_POLARITY_LOW, CLOCK_PHASE_LEADING, true, true>(uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_LSB_FIRST, CLOCK_POLARITY_LOW, CLOCK_PHASE_TRAILING, false, true>(uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_LSB_FIRST, CLOCK_POLARITY_LOW, CLOCK_PHASE_TRAILING, true, false>(uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_LSB_FIRST, CLOCK_POLARITY_LOW, CLOCK_PHASE_TRAILING, true, true>(uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_LSB_FIRST, CLOCK_POLARITY_HIGH, CLOCK_PHASE_LEADING, false, true>(uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_LSB_FIRST, CLOCK_POLARITY_HIGH, CLOCK_PHASE_LEADING, true, false>(uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_LSB_FIRST, CLOCK_POLARITY_HIGH, CLOCK_PHASE_LEADING, true, true>(uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_LSB_FIRST, CLOCK_POLARITY_HIGH, CLOCK_PHASE_TRAILING, false, true>(uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_LSB_FIRST, CLOCK_POLARITY_HIGH, CLOCK_PHASE_TRAILING, true, false>(uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_LSB_FIRST, CLOCK_POLARITY_HIGH, CLOCK_PHASE_TRAILING, true, true>(uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_MSB_FIRST, CLOCK_POLARITY_LOW, CLOCK_PHASE_LEADING, false, true>(uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_MSB_FIRST, CLOCK_POLARITY_LOW, CLOCK_PHASE_LEADING, true, false>(uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_MSB_FIRST, CLOCK_POLARITY_LOW, CLOCK_PHASE_LEADING, true, true>(uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_MSB_FIRST, CLOCK_POLARITY_LOW, CLOCK_PHASE_TRAILING, false, true>(uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_MSB_FIRST, CLOCK_POLARITY_LOW, CLOCK_PHASE_TRAILING, true, false>(uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_MSB_FIRST, CLOCK_POLARITY_LOW, CLOCK_PHASE_TRAILING, true, true>(uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_MSB_FIRST, CLOCK_POLARITY_HIGH, CLOCK_PHASE_LEADING, false, true>(uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_MSB_FIRST, CLOCK_POLARITY_HIGH, CLOCK_PHASE_LEADING, true, false>(uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_MSB_FIRST, CLOCK_POLARITY_HIGH, CLOCK_PHASE_LEADING, true, true>(uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_MSB_FIRST, CLOCK_POLARITY_HIGH, CLOCK_PHASE_TRAILING, false, true>(uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_MSB_FIRST, CLOCK_POLARITY_HIGH, CLOCK_PHASE_TRAILING, true, false>(uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_MSB_FIRST, CLOCK_POLARITY_HIGH, CLOCK_PHASE_TRAILING, true, true>(uint8_t data);

}  // namespace spi
}  // namespace esphome
