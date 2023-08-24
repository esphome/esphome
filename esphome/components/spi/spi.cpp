#include "spi.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"


namespace esphome {
namespace spi {

static uint8_t spi_bus_num = 0;
const char *const TAG = "spi";

SPIDelegateDummy SPIDelegate::null_delegate;
GPIOPin *NullPin::null_pin = new NullPin();

SPIDelegate *SPIComponent::register_device(SPIClient *device,
                                           SPIMode mode,
                                           SPIBitOrder bit_order,
                                           uint32_t data_rate,
                                           GPIOPin *cs_pin) {

  if (this->devices_.count(device) != 0) {
    ESP_LOGE(TAG, "SPI device already registered");
    return this->devices_[device];
  }
  SPIDelegate *delegate;
  if (this->spi_channel_ == nullptr) {
    // NOLINT
    delegate = new SPIDelegateBitBash(data_rate, bit_order, mode, cs_pin, this->clk_pin_, this->sdo_pin_,
                                                   this->sdi_pin_);
  } else
    delegate = new SPIHWDelegate(this->spi_channel_, data_rate, bit_order, mode, cs_pin); //NOLINT
  this->devices_[device] = delegate;
  return delegate;
}

void SPIComponent::unregister_device(SPIClient *device) {
  if (this->devices_.count(device) == 0) {
    esph_log_e(TAG, "SPI device not registered");
    return;
  }
  delete this->devices_[device];  //NOLINT
  this->devices_.erase(device);
}

void SPIComponent::setup() {
  ESP_LOGD(TAG, "Setting up SPI bus...");
  this->clk_pin_->setup();
  this->clk_pin_->digital_write(true);

  bool use_hw_spi = !this->force_sw_;
  const bool has_miso = this->sdi_pin_ != nullptr;
  const bool has_mosi = this->sdo_pin_ != nullptr;
  int8_t clk_pin = -1, miso_pin = -1, mosi_pin = -1;

  if (!this->clk_pin_->is_internal())
    use_hw_spi = false;
  if (has_miso && !sdi_pin_->is_internal())
    use_hw_spi = false;
  if (has_mosi && !sdo_pin_->is_internal())
    use_hw_spi = false;
  if (use_hw_spi) {
    auto *clk_internal = (InternalGPIOPin *) clk_pin_;
    auto *miso_internal = (InternalGPIOPin *) sdi_pin_;
    auto *mosi_internal = (InternalGPIOPin *) sdo_pin_;

    if (clk_internal->is_inverted())
      use_hw_spi = false;
    if (has_miso && miso_internal->is_inverted())
      use_hw_spi = false;
    if (has_mosi && mosi_internal->is_inverted())
      use_hw_spi = false;

    if (use_hw_spi) {
      clk_pin = clk_internal->get_pin();
      miso_pin = has_miso ? miso_internal->get_pin() : -1;
      mosi_pin = has_mosi ? mosi_internal->get_pin() : -1;
    }
  }

#ifdef USE_ESP8266
  if (!(clk_pin == 6 && miso_pin == 7 && mosi_pin == 8) &&
      !(clk_pin == 14 && (!has_miso || miso_pin == 12) && (!has_mosi || mosi_pin == 13)))
    use_hw_spi = false;

  if (use_hw_spi) {
    this->spi_channel_ = &SPI;
    this->spi_channel_->pins(clk_pin, miso_pin, mosi_pin, 0);
    this->spi_channel_->begin();
    return;
  }
#endif  // USE_ESP8266
#ifdef USE_ESP32

  if (spi_bus_num >= 2) {
    use_hw_spi = false;
  }

  if (use_hw_spi) {
    if (spi_bus_num == 0) {
      this->spi_channel_ = &SPI;
    } else {
#if defined(USE_ESP32_VARIANT_ESP32C3) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || \
    defined(USE_ESP32_VARIANT_ESP32C2) || defined(USE_ESP32_VARIANT_ESP32C6)
      this->hw_spi_ = new SPIClass(FSPI);  // NOLINT(cppcoreguidelines-owning-memory)
#else
      this->spi_channel_ = new SPIClass(HSPI);  // NOLINT(cppcoreguidelines-owning-memory)
#endif  // USE_ESP32_VARIANT
    }
    spi_bus_num++;
    this->spi_channel_->begin(clk_pin, miso_pin, mosi_pin);
    return;
  }
#endif  // USE_ESP32
#ifdef USE_RP2040
  static uint8_t spi_bus_num = 0;
  if (spi_bus_num >= 2) {
    use_hw_spi = false;
  }
  if (use_hw_spi) {
    SPIClassRP2040 *spi;
    if (spi_bus_num == 0) {
      spi = &SPI;
    } else {
      spi = &SPI1;
    }
    spi_bus_num++;

    if (miso_pin != -1)
      spi->setRX(miso_pin);
    if (mosi_pin != -1)
      spi->setTX(mosi_pin);
    spi->setSCK(clk_pin);
    this->spi_channel_ = spi;
    this->hw_spi_->begin();
    return;
  }
#endif  // USE_RP2040

  if (this->sdi_pin_ != nullptr) {
    this->sdi_pin_->setup();
  }
  if (this->sdo_pin_ != nullptr) {
    this->sdo_pin_->setup();
    this->sdo_pin_->digital_write(false);
  }
}

void SPIComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SPI bus:");
  LOG_PIN("  CLK Pin: ", this->clk_pin_)
  LOG_PIN("  MISO Pin: ", this->sdi_pin_)
  LOG_PIN("  MOSI Pin: ", this->sdo_pin_)
  ESP_LOGCONFIG(TAG, "  Using HW SPI: %s", YESNO(this->spi_channel_ != nullptr));
}

float SPIComponent::get_setup_priority() const { return setup_priority::BUS; }

void SPIDelegateDummy::begin_transaction() {
  ESP_LOGE(TAG, "SPIDevice not initialised - did you call spi_setup()?");
}

void SPIClient::enable() {
  this->delegate_->begin_transaction();
}

void SPIClient::disable() {
  this->delegate_->end_transaction();
}

}  // namespace spi
}  // namespace esphome
