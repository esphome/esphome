#include "spi.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome::spi {

ESPHOME_LOG_TAG(TAG, "spi");

static constinit SPIDelegateDummy null_delegate;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
SPIDelegate *const SPIDelegate::NULL_DELEGATE = &null_delegate;

bool SPIDelegate::is_ready() { return true; }

SPIDelegate *SPIComponent::register_device(SPIClient *device, SPIMode mode, SPIBitOrder bit_order, uint32_t data_rate,
                                           GPIOPin *cs_pin, bool release_device, bool write_only) {
  if (device->delegate_ != SPIDelegate::NULL_DELEGATE) {
    ESP_LOGE(TAG, "Device already registered");
    return device->delegate_;
  }
  if (!this->is_ready()) {
    return SPIDelegate::NULL_DELEGATE;  // the bus failed in setup(); the device stays not ready
  }
#ifdef USE_SPI_SOFTWARE
  if (!this->using_hw_) {
    return new SPIDelegateBitBash(data_rate, bit_order, mode, cs_pin, this->clk_pin_, this->sdo_pin_,  // NOLINT
                                  this->sdi_pin_);
  }
#endif
  return this->new_hw_delegate_(data_rate, bit_order, mode, cs_pin, release_device, write_only);
}

void SPIComponent::unregister_device(SPIClient *device) {
  if (device->delegate_ == SPIDelegate::NULL_DELEGATE) {
    if (this->is_ready()) {
      ESP_LOGE(TAG, "Device not registered");
    }
    return;
  }
  delete device->delegate_;  // NOLINT
  device->delegate_ = SPIDelegate::NULL_DELEGATE;
}

void SPIComponent::setup() {
  if (this->sdo_pin_ == nullptr)
    this->sdo_pin_ = NullPin::NULL_PIN;
  if (this->sdi_pin_ == nullptr)
    this->sdi_pin_ = NullPin::NULL_PIN;
  if (this->clk_pin_ == nullptr) {
    ESP_LOGE(TAG, "No clock pin");
    this->mark_failed();
    return;
  }

#ifdef USE_SPI_SOFTWARE
  if (!this->using_hw_) {
    this->clk_pin_->setup();
    this->clk_pin_->digital_write(true);
    this->sdo_pin_->setup();
    this->sdi_pin_->setup();
    return;
  }
#endif
  if (!this->setup_hw_bus_())
    this->mark_failed(LOG_STR("Unable to allocate SPI interface"));
}

void SPIComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SPI bus:");
  LOG_PIN("  CLK Pin: ", this->clk_pin_);
  LOG_PIN("  SDI Pin: ", this->sdi_pin_);
  LOG_PIN("  SDO Pin: ", this->sdo_pin_);
  for (size_t i = 0; i != this->data_pins_.size(); i++) {
    ESP_LOGCONFIG(TAG, "  Data pin %zu: GPIO%d", i, this->data_pins_[i]);
  }
  if (!this->is_ready()) {
    return;
  }
  if (this->using_hw_) {
    ESP_LOGCONFIG(TAG, "  Using HW SPI: %s", this->interface_name_);
  } else {
    ESP_LOGCONFIG(TAG, "  Using software SPI");
  }
}

void SPIDelegateDummy::begin_transaction() { ESP_LOGE(TAG, "SPIDevice not initialised - did you call spi_setup()?"); }

#ifdef USE_SPI_SOFTWARE
uint8_t SPIDelegateBitBash::transfer(uint8_t data) { return this->transfer_(data, 8); }

void SPIDelegateBitBash::write(uint16_t data, size_t num_bits) { this->transfer_(data, num_bits); }

uint16_t SPIDelegateBitBash::transfer_(uint16_t data, size_t num_bits) {
  // Clock starts out at idle level
  this->clk_pin_->digital_write(clock_polarity_);
  uint16_t out_data = 0;

  for (uint8_t i = 0; i != num_bits; i++) {
    const uint8_t shift = this->bit_order_ == BIT_ORDER_MSB_FIRST ? num_bits - 1 - i : i;

    if (clock_phase_ == CLOCK_PHASE_LEADING) {
      // sampling on leading edge
      this->sdo_pin_->digital_write(data & (1 << shift));
      this->cycle_clock_();
      out_data |= uint16_t(this->sdi_pin_->digital_read()) << shift;
      this->clk_pin_->digital_write(!this->clock_polarity_);
      this->cycle_clock_();
      this->clk_pin_->digital_write(this->clock_polarity_);
    } else {
      // sampling on trailing edge
      this->cycle_clock_();
      this->clk_pin_->digital_write(!this->clock_polarity_);
      this->sdo_pin_->digital_write(data & (1 << shift));
      this->cycle_clock_();
      out_data |= uint16_t(this->sdi_pin_->digital_read()) << shift;
      this->clk_pin_->digital_write(this->clock_polarity_);
    }
  }
  App.feed_wdt();
  return out_data;
}
#endif  // USE_SPI_SOFTWARE

#if !defined(USE_ESP32) && !defined(USE_ARDUINO) && !defined(USE_ZEPHYR)
// Stub for unsupported platforms (host, etc.) - hardware SPI is unavailable
bool SPIComponent::setup_hw_bus_() { return false; }

SPIDelegate *SPIComponent::new_hw_delegate_(uint32_t data_rate, SPIBitOrder bit_order, SPIMode mode, GPIOPin *cs_pin,
                                            bool release_device, bool write_only) {
  return SPIDelegate::NULL_DELEGATE;
}
#endif

}  // namespace esphome::spi
