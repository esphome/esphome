#include "spi.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#ifdef USE_ESP32
#include "driver/gpio.h"
#endif

namespace esphome::spi {

const char *const TAG = "spi";

SPIDelegate *const SPIDelegate::NULL_DELEGATE =  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
    new SPIDelegateDummy();
// https://bugs.llvm.org/show_bug.cgi?id=48040

bool SPIDelegate::is_ready() { return true; }

GPIOPin *const NullPin::NULL_PIN = new NullPin();  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

SPIDelegate *SPIComponent::register_device(SPIClient *device, SPIMode mode, SPIBitOrder bit_order, uint32_t data_rate,
                                           GPIOPin *cs_pin, bool release_device, bool write_only) {
  if (this->devices_.contains(device)) {
    ESP_LOGE(TAG, "Device already registered");
    return this->devices_[device];
  }
  SPIDelegate *delegate =
      this->spi_bus_->get_delegate(data_rate, bit_order, mode, cs_pin, release_device, write_only);  // NOLINT
  this->devices_[device] = delegate;
  return delegate;
}

void SPIComponent::unregister_device(SPIClient *device) {
  if (!this->devices_.contains(device)) {
    esph_log_e(TAG, "Device not registered");
    return;
  }
  delete this->devices_[device];  // NOLINT
  this->devices_.erase(device);
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

  if (this->using_hw_) {
    this->spi_bus_ =
        SPIComponent::get_bus(this->interface_, this->clk_pin_, this->sdo_pin_, this->sdi_pin_, this->data_pins_);
    if (this->spi_bus_ == nullptr) {
      ESP_LOGE(TAG, "Unable to allocate SPI interface");
      this->mark_failed();
    }
  } else {
    this->spi_bus_ = new SPIBus(this->clk_pin_, this->sdo_pin_, this->sdi_pin_, this->data_pins_);  // NOLINT
    this->clk_pin_->setup();
    this->clk_pin_->digital_write(true);
    if (this->data_pins_.empty()) {
      this->sdo_pin_->setup();
      this->sdi_pin_->setup();
    }
  }
}

void SPIComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SPI bus:");
  LOG_PIN("  CLK Pin: ", this->clk_pin_);
  LOG_PIN("  SDI Pin: ", this->sdi_pin_);
  LOG_PIN("  SDO Pin: ", this->sdo_pin_);
  for (size_t i = 0; i != this->data_pins_.size(); i++) {
    ESP_LOGCONFIG(TAG, "  Data pin %zu: GPIO%d", i, this->data_pins_[i]);
  }
  if (this->spi_bus_->is_hw()) {
    ESP_LOGCONFIG(TAG, "  Using HW SPI: %s", this->interface_name_);
  } else {
    ESP_LOGCONFIG(TAG, "  Using software SPI");
  }
}

void SPIDelegateDummy::begin_transaction() { ESP_LOGE(TAG, "SPIDevice not initialised - did you call spi_setup()?"); }

uint8_t SPIDelegateBitBash::transfer(uint8_t data) { return this->transfer_(data, 8); }

void SPIDelegateBitBash::write(uint16_t data, size_t num_bits) { this->transfer_(data, num_bits); }

uint16_t SPIDelegateBitBash::transfer_(uint16_t data, size_t num_bits) {
  // Clock starts out at idle level
  this->clk_pin_->digital_write(clock_polarity_);
  uint16_t out_data = 0;

  for (uint8_t i = 0; i != num_bits; i++) {
    uint8_t shift;
    if (bit_order_ == BIT_ORDER_MSB_FIRST) {
      shift = num_bits - 1 - i;
    } else {
      shift = i;
    }

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

#ifdef USE_ESP32
void SPIDelegateMultiBitBash::setup_bus_() {
  if (this->ready_)
    return;

  const int clk_pin = Utility::get_internal_pin_no(this->clk_pin_);
  if (clk_pin < 0 || clk_pin > 31) {
    ESP_LOGE(TAG, "Software quad/octal SPI clock pin must be an internal GPIO0-31 pin");
    return;
  }
  this->clk_mask_ = 1UL << clk_pin;
  if (Utility::is_inverted(this->clk_pin_)) {
    this->clock_polarity_ = this->clock_polarity_ == CLOCK_POLARITY_HIGH ? CLOCK_POLARITY_LOW : CLOCK_POLARITY_HIGH;
  }
  this->clk_pin_->setup();
  if (this->clock_polarity_) {
    GPIO.out_w1ts = this->clk_mask_;
  } else {
    GPIO.out_w1tc = this->clk_mask_;
  }

  if (this->data_pins_.size() != 4 && this->data_pins_.size() != 8) {
    ESP_LOGE(TAG, "Software multi-bit SPI requires exactly 4 or 8 data pins");
    return;
  }

  this->data_clear_mask_ = 0;
  for (uint8_t pin : this->data_pins_) {
    if (pin > 31) {
      ESP_LOGE(TAG, "Software quad/octal SPI data pins must be internal GPIO0-31 pins");
      return;
    }
    gpio_reset_pin(static_cast<gpio_num_t>(pin));
    gpio_set_direction(static_cast<gpio_num_t>(pin), GPIO_MODE_OUTPUT);
    GPIO.out_w1tc = 1UL << pin;
    this->data_clear_mask_ |= 1UL << pin;
  }

  for (uint16_t value = 0; value < this->data_set_masks_.size(); value++) {
    uint32_t mask = 0;
    for (uint8_t bit = 0; bit < this->data_pins_.size(); bit++) {
      if (value & (1 << bit))
        mask |= 1UL << this->data_pins_[bit];
    }
    this->data_set_masks_[value] = mask;
  }
  this->last_transition_ = arch_get_cpu_cycle_count();
  this->ready_ = true;
}

uint8_t SPIDelegateMultiBitBash::transfer(uint8_t data) {
  this->write_array(&data, 1);
  return 0;
}

void SPIDelegateMultiBitBash::write_multi_width_(const uint8_t *data, size_t length, uint8_t bus_width) {
  if (!this->ready_) {
    ESP_LOGE(TAG, "Software multi-bit SPI bus is not ready");
    return;
  }
  // This is deliberately expanded to avoid call and branch overhead.
  if (bus_width == 8 && this->data_pins_.size() >= 8) {
    if (this->clock_polarity_) {
      while (length-- != 0) {
        GPIO.out_w1tc = this->data_clear_mask_;
        GPIO.out_w1ts = this->data_set_masks_[*data++];
        this->cycle_clock_();
        GPIO.out_w1tc = this->clk_mask_;
        this->cycle_clock_();
        GPIO.out_w1ts = this->clk_mask_;
      }
    } else {
      while (length-- != 0) {
        GPIO.out_w1tc = this->data_clear_mask_;
        GPIO.out_w1ts = this->data_set_masks_[*data++];
        this->cycle_clock_();
        GPIO.out_w1ts = this->clk_mask_;
        this->cycle_clock_();
        GPIO.out_w1tc = this->clk_mask_;
      }
    }
  } else if (bus_width == 4 && this->data_pins_.size() >= 4) {
    if (this->clock_polarity_) {
      while (length-- != 0) {
        const uint8_t value = *data++;
        GPIO.out_w1tc = this->data_clear_mask_;
        GPIO.out_w1ts = this->data_set_masks_[value >> 4];
        this->cycle_clock_();
        GPIO.out_w1tc = this->clk_mask_;
        this->cycle_clock_();
        GPIO.out_w1ts = this->clk_mask_;
        GPIO.out_w1tc = this->data_clear_mask_;
        GPIO.out_w1ts = this->data_set_masks_[value & 0x0F];
        this->cycle_clock_();
        GPIO.out_w1tc = this->clk_mask_;
        this->cycle_clock_();
        GPIO.out_w1ts = this->clk_mask_;
      }
    } else {
      while (length-- != 0) {
        const uint8_t value = *data++;
        GPIO.out_w1tc = this->data_clear_mask_;
        GPIO.out_w1ts = this->data_set_masks_[value >> 4];
        this->cycle_clock_();
        GPIO.out_w1ts = this->clk_mask_;
        this->cycle_clock_();
        GPIO.out_w1tc = this->clk_mask_;
        GPIO.out_w1tc = this->data_clear_mask_;
        GPIO.out_w1ts = this->data_set_masks_[value & 0x0F];
        this->cycle_clock_();
        GPIO.out_w1ts = this->clk_mask_;
        this->cycle_clock_();
        GPIO.out_w1tc = this->clk_mask_;
      }
    }
  } else {
    this->write_bits_(0, 0);
    ESP_LOGE(TAG, "Unsupported software SPI bus width %u for %u data pins", bus_width,
             (unsigned) this->data_pins_.size());
  }
  App.feed_wdt();
}

void SPIDelegateMultiBitBash::write_bits_(uint32_t data, size_t num_bits) {
  if (!this->ready_)
    return;
  for (size_t i = 0; i < num_bits; i++) {
    const size_t shift = this->bit_order_ == BIT_ORDER_MSB_FIRST ? num_bits - 1 - i : i;
    GPIO.out_w1tc = this->data_clear_mask_;
    GPIO.out_w1ts = this->data_set_masks_[(data >> shift) & 0x01];
    this->cycle_clock_();
    if (this->clock_polarity_) {
      GPIO.out_w1tc = this->clk_mask_;
      this->cycle_clock_();
      GPIO.out_w1ts = this->clk_mask_;
    } else {
      GPIO.out_w1ts = this->clk_mask_;
      this->cycle_clock_();
      GPIO.out_w1tc = this->clk_mask_;
    }
  }
}

void SPIDelegateMultiBitBash::write_cmd_addr_data(size_t cmd_bits, uint32_t cmd, size_t addr_bits, uint32_t address,
                                                  const uint8_t *data, size_t length, uint8_t bus_width) {
  this->setup_bus_();
  if (!this->ready_)
    return;
  if (cmd_bits != 0)
    this->write_bits_(cmd, cmd_bits);
  if (addr_bits != 0)
    this->write_bits_(address, addr_bits);
  if (data != nullptr && length != 0)
    this->write_multi_width_(data, length, bus_width);
}

void SPIDelegateMultiBitBash::write_array(const uint8_t *ptr, size_t length) {
  this->setup_bus_();
  this->write_multi_width_(ptr, length, this->data_pins_.size());
}
#endif

#if !defined(USE_ESP32) && !defined(USE_ARDUINO)
// Stub for unsupported platforms (host, Zephyr, etc.) - hardware SPI is unavailable
SPIBus *SPIComponent::get_bus(SPIInterface interface, GPIOPin *clk, GPIOPin *sdo, GPIOPin *sdi,
                              const std::vector<uint8_t> &data_pins) {
  return nullptr;
}
#endif

}  // namespace esphome::spi
