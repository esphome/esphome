#include "spi.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace spi {

static uint8_t spi_bus_num = 0;
const char *const TAG = "spi";

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
  this->clk_pin_->setup();
  this->clk_pin_->digital_write(true);

  if (this->sdo_pin_ == nullptr)
    this->sdo_pin_ = NullPin::NULL_PIN;
  if (this->sdi_pin_ == nullptr)
    this->sdi_pin_ = NullPin::NULL_PIN;
  if (this->clk_pin_ == nullptr) {
    ESP_LOGE(TAG, "No clock pin for SPI");
    this->mark_failed();
    return;
  }
  bool use_hw_spi = !this->force_sw_;

  if (!this->clk_pin_->is_internal() || Utility::is_pin_inverted(this->clk_pin_))
    use_hw_spi = false;
  if (!sdo_pin_->is_internal() || Utility::is_pin_inverted(this->sdo_pin_))
    use_hw_spi = false;
  if (!sdi_pin_->is_internal() || Utility::is_pin_inverted(this->sdi_pin_))
    use_hw_spi = false;

#ifdef USE_ESP8266
  int8_t clk_pin = Utility::get_pin_no(this->clk_pin_);
  int8_t sdo_pin = Utility::get_pin_no(this->sdo_pin_);
  int8_t sdi_pin = Utility::get_pin_no(this->sdi_pin_);
  if (!(clk_pin == 6 && sdi_pin == 7 && sdo_pin == 8) && !(clk_pin == 14 && sdi_pin == 12 && sdo_pin == 13)) {
    use_hw_spi = false;
  }
#endif

  SPIBus *bus = nullptr;
  if (use_hw_spi)
    bus = SPIComponent::get_next_bus(spi_bus_num++, this->clk_pin_, this->sdo_pin_, this->sdi_pin_);
  if (bus != nullptr) {
    this->spi_bus_ = bus;
  } else {
    this->spi_bus_ = new SPIBus(this->clk_pin_, this->sdo_pin_, this->sdi_pin_);  // NOLINT
    this->sdo_pin_->setup();
    this->sdi_pin_->setup();
  }
}

void SPIComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SPI bus:");
  LOG_PIN("  CLK Pin: ", this->clk_pin_)
  LOG_PIN("  SDI Pin: ", this->sdi_pin_)
  LOG_PIN("  SDO Pin: ", this->sdo_pin_)
  ESP_LOGCONFIG(TAG, "  Using HW SPI: %s", YESNO(this->spi_bus_->is_hw()));
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
