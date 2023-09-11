#include "spi.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace spi {

const char *const TAG = "spi";

SPIDelegate *const SPIDelegate::NULL_DELEGATE =  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
    new SPIDelegateDummy();
// https://bugs.llvm.org/show_bug.cgi?id=48040

bool SPIDelegate::is_ready() { return true; }

GPIOPin *const NullPin::NULL_PIN = new NullPin();  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

SPIDelegate *SPIComponent::register_device(SPIClient *device, SPIMode mode, SPIBitOrder bit_order, uint32_t data_rate,
                                           GPIOPin *cs_pin) {
  if (this->devices_.count(device) != 0) {
    ESP_LOGE(TAG, "SPI device already registered");
    return this->devices_[device];
  }
  SPIDelegate *delegate = this->spi_bus_->get_delegate(data_rate, bit_order, mode, cs_pin);  // NOLINT
  this->devices_[device] = delegate;
  return delegate;
}

void SPIComponent::unregister_device(SPIClient *device) {
  if (this->devices_.count(device) == 0) {
    esph_log_e(TAG, "SPI device not registered");
    return;
  }
  delete this->devices_[device];  // NOLINT
  this->devices_.erase(device);
}

void SPIComponent::setup() {
  ESP_LOGD(TAG, "Setting up SPI bus...");

  if (this->sdo_pin_ == nullptr)
    this->sdo_pin_ = NullPin::NULL_PIN;
  if (this->sdi_pin_ == nullptr)
    this->sdi_pin_ = NullPin::NULL_PIN;
  if (this->clk_pin_ == nullptr) {
    ESP_LOGE(TAG, "No clock pin for SPI");
    this->mark_failed();
    return;
  }

  if (this->using_hw_) {
    this->spi_bus_ = SPIComponent::get_bus(this->interface_, this->clk_pin_, this->sdo_pin_, this->sdi_pin_);
    if (this->spi_bus_ == nullptr) {
      ESP_LOGE(TAG, "Unable to allocate SPI interface");
      this->mark_failed();
    }
  } else {
    this->spi_bus_ = new SPIBus(this->clk_pin_, this->sdo_pin_, this->sdi_pin_);  // NOLINT
    this->clk_pin_->setup();
    this->clk_pin_->digital_write(true);
    this->sdo_pin_->setup();
    this->sdi_pin_->setup();
  }
}

void SPIComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SPI bus:");
  LOG_PIN("  CLK Pin: ", this->clk_pin_)
  LOG_PIN("  SDI Pin: ", this->sdi_pin_)
  LOG_PIN("  SDO Pin: ", this->sdo_pin_)
  if (this->spi_bus_->is_hw()) {
    ESP_LOGCONFIG(TAG, "  Using HW SPI: %s", this->interface_name_);
  } else {
    ESP_LOGCONFIG(TAG, "  Using software SPI");
  }
}

void SPIDelegateDummy::begin_transaction() { ESP_LOGE(TAG, "SPIDevice not initialised - did you call spi_setup()?"); }

uint8_t SPIDelegateBitBash::transfer(uint8_t data) {
  // Clock starts out at idle level
  this->clk_pin_->digital_write(clock_polarity_);
  uint8_t out_data = 0;

  for (uint8_t i = 0; i < 8; i++) {
    uint8_t shift;
    if (bit_order_ == BIT_ORDER_MSB_FIRST) {
      shift = 7 - i;
    } else {
      shift = i;
    }

    if (clock_phase_ == CLOCK_PHASE_LEADING) {
      // sampling on leading edge
      this->sdo_pin_->digital_write(data & (1 << shift));
      this->cycle_clock_();
      out_data |= uint8_t(this->sdi_pin_->digital_read()) << shift;
      this->clk_pin_->digital_write(!this->clock_polarity_);
      this->cycle_clock_();
      this->clk_pin_->digital_write(this->clock_polarity_);
    } else {
      // sampling on trailing edge
      this->cycle_clock_();
      this->clk_pin_->digital_write(!this->clock_polarity_);
      this->sdo_pin_->digital_write(data & (1 << shift));
      this->cycle_clock_();
      out_data |= uint8_t(this->sdi_pin_->digital_read()) << shift;
      this->clk_pin_->digital_write(this->clock_polarity_);
    }
  }
  App.feed_wdt();
  return out_data;
}

}  // namespace spi
}  // namespace esphome
