#include "spi.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/application.h"

#ifdef USE_ESP_IDF
#include <hal/cpu_hal.h>
#endif

namespace esphome {
namespace spi {

static const char *const TAG = "spi";

void ICACHE_RAM_ATTR HOT SPIComponent::disable() {
#ifdef USE_SPI_ARDUINO_BACKEND
  if (this->hw_spi_ != nullptr) {
    this->hw_spi_->endTransaction();
  }
#endif  // USE_SPI_ARDUINO_BACKEND
  if (this->active_cs_) {
    this->active_cs_->digital_write(true);
    this->active_cs_ = nullptr;
  }
}
void SPIComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SPI bus...");
  this->clk_->setup();
  this->clk_->digital_write(true);

#ifdef USE_SPI_ARDUINO_BACKEND
  bool use_hw_spi = true;
  if (!this->clk_->is_internal() || this->clk_->is_inverted())
    use_hw_spi = false;
  const bool has_miso = this->miso_ != nullptr;
  const bool has_mosi = this->mosi_ != nullptr;
  if (has_miso && (this->miso_->is_inverted() || !this->miso_->is_internal()))
    use_hw_spi = false;
  if (has_mosi && (this->mosi_->is_inverted() || !this->mosi_->is_internal()))
    use_hw_spi = false;
  int8_t clk_pin = -1, miso_pin = -1, mosi_pin = -1;
  if (use_hw_spi) {
    clk_pin = ((InternalGPIOPin *) this->clk_)->get_pin();
    miso_pin = has_miso ? ((InternalGPIOPin *) this->miso_)->get_pin() : -1;
    mosi_pin = has_mosi ? ((InternalGPIOPin *) this->mosi_)->get_pin() : -1;
  }
#ifdef USE_ESP8266
  if (clk_pin == 6 && miso_pin == 7 && mosi_pin == 8) {
    // pass
  } else if (clk_pin == 14 && (!has_miso || miso_pin == 12) && (!has_mosi || mosi_pin == 13)) {
    // pass
  } else {
    use_hw_spi = false;
  }

  if (use_hw_spi) {
    this->hw_spi_ = &SPI;
    this->hw_spi_->pins(clk_pin, miso_pin, mosi_pin, 0);
    this->hw_spi_->begin();
    return;
  }
#endif  // USE_ESP8266
#ifdef USE_ESP32
  static uint8_t spi_bus_num = 0;
  if (spi_bus_num >= 2) {
    use_hw_spi = false;
  }

  if (use_hw_spi) {
    if (spi_bus_num == 0) {
      this->hw_spi_ = &SPI;
    } else {
      this->hw_spi_ = new SPIClass(VSPI);  // NOLINT(cppcoreguidelines-owning-memory)
    }
    spi_bus_num++;
    this->hw_spi_->begin(clk_pin, miso_pin, mosi_pin);
    return;
  }
#endif  // USE_ESP32
#endif  // USE_SPI_ARDUINO_BACKEND

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
#ifdef USE_SPI_ARDUINO_BACKEND
  ESP_LOGCONFIG(TAG, "  Using HW SPI: %s", YESNO(this->hw_spi_ != nullptr));
#endif  // USE_SPI_ARDUINO_BACKEND
}
float SPIComponent::get_setup_priority() const { return setup_priority::BUS; }

void SPIComponent::cycle_clock_(bool value) {
#if defined(USE_ESP8266) || defined(USE_ESP32_ARDUINO)
  uint32_t start = ESP.getCycleCount();                    // NOLINT(readability-static-accessed-through-instance)
  while (start - ESP.getCycleCount() < this->wait_cycle_)  // NOLINT(readability-static-accessed-through-instance)
    ;
  this->clk_->digital_write(value);
  start += this->wait_cycle_;
  while (start - ESP.getCycleCount() < this->wait_cycle_)  // NOLINT(readability-static-accessed-through-instance)
    ;
#elif defined(USE_ESP_IDF)
  uint32_t start = cpu_hal_get_cycle_count();
  while (start - cpu_hal_get_cycle_count() < this->wait_cycle_)
    ;
  this->clk_->digital_write(value);
  start += this->wait_cycle_;
  while (start - cpu_hal_get_cycle_count() < this->wait_cycle_)
    ;
#else
#error "No cycle source implemented"
#endif
}

// NOLINTNEXTLINE
#pragma GCC optimize("unroll-loops")
// NOLINTNEXTLINE
#pragma GCC optimize("O2")

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
        out_data |= uint8_t(this->miso_->digital_read()) << shift;
      }

      this->cycle_clock_(CLOCK_POLARITY);
    } else {
      // sampling on trailing edge
      this->cycle_clock_(!CLOCK_POLARITY);

      if (WRITE) {
        this->mosi_->digital_write(data & (1 << shift));
      }

      // SAMPLE!
      this->cycle_clock_(CLOCK_POLARITY);

      if (READ) {
        out_data |= uint8_t(this->miso_->digital_read()) << shift;
      }
    }
  }

  App.feed_wdt();

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
//     print(f"template uint8_t SPIComponent::transfer_<{b}, {cpol}, {cph}, {cpp_bool[r]}, {cpp_bool[w]}>(uint8_t
//     data);")

template uint8_t SPIComponent::transfer_<BIT_ORDER_LSB_FIRST, CLOCK_POLARITY_LOW, CLOCK_PHASE_LEADING, false, true>(
    uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_LSB_FIRST, CLOCK_POLARITY_LOW, CLOCK_PHASE_LEADING, true, false>(
    uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_LSB_FIRST, CLOCK_POLARITY_LOW, CLOCK_PHASE_LEADING, true, true>(
    uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_LSB_FIRST, CLOCK_POLARITY_LOW, CLOCK_PHASE_TRAILING, false, true>(
    uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_LSB_FIRST, CLOCK_POLARITY_LOW, CLOCK_PHASE_TRAILING, true, false>(
    uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_LSB_FIRST, CLOCK_POLARITY_LOW, CLOCK_PHASE_TRAILING, true, true>(
    uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_LSB_FIRST, CLOCK_POLARITY_HIGH, CLOCK_PHASE_LEADING, false, true>(
    uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_LSB_FIRST, CLOCK_POLARITY_HIGH, CLOCK_PHASE_LEADING, true, false>(
    uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_LSB_FIRST, CLOCK_POLARITY_HIGH, CLOCK_PHASE_LEADING, true, true>(
    uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_LSB_FIRST, CLOCK_POLARITY_HIGH, CLOCK_PHASE_TRAILING, false, true>(
    uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_LSB_FIRST, CLOCK_POLARITY_HIGH, CLOCK_PHASE_TRAILING, true, false>(
    uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_LSB_FIRST, CLOCK_POLARITY_HIGH, CLOCK_PHASE_TRAILING, true, true>(
    uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_MSB_FIRST, CLOCK_POLARITY_LOW, CLOCK_PHASE_LEADING, false, true>(
    uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_MSB_FIRST, CLOCK_POLARITY_LOW, CLOCK_PHASE_LEADING, true, false>(
    uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_MSB_FIRST, CLOCK_POLARITY_LOW, CLOCK_PHASE_LEADING, true, true>(
    uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_MSB_FIRST, CLOCK_POLARITY_LOW, CLOCK_PHASE_TRAILING, false, true>(
    uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_MSB_FIRST, CLOCK_POLARITY_LOW, CLOCK_PHASE_TRAILING, true, false>(
    uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_MSB_FIRST, CLOCK_POLARITY_LOW, CLOCK_PHASE_TRAILING, true, true>(
    uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_MSB_FIRST, CLOCK_POLARITY_HIGH, CLOCK_PHASE_LEADING, false, true>(
    uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_MSB_FIRST, CLOCK_POLARITY_HIGH, CLOCK_PHASE_LEADING, true, false>(
    uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_MSB_FIRST, CLOCK_POLARITY_HIGH, CLOCK_PHASE_LEADING, true, true>(
    uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_MSB_FIRST, CLOCK_POLARITY_HIGH, CLOCK_PHASE_TRAILING, false, true>(
    uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_MSB_FIRST, CLOCK_POLARITY_HIGH, CLOCK_PHASE_TRAILING, true, false>(
    uint8_t data);
template uint8_t SPIComponent::transfer_<BIT_ORDER_MSB_FIRST, CLOCK_POLARITY_HIGH, CLOCK_PHASE_TRAILING, true, true>(
    uint8_t data);

}  // namespace spi
}  // namespace esphome
