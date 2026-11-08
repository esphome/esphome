#include "spi.h"

namespace esphome::spi {
#if defined(USE_ARDUINO) && !defined(USE_ESP32)

ESPHOME_LOG_TAG(TAG, "spi");
class SPIDelegateHw : public SPIDelegate {
 public:
  SPIDelegateHw(SPIInterface channel, uint32_t data_rate, SPIBitOrder bit_order, SPIMode mode, GPIOPin *cs_pin)
      : SPIDelegate(data_rate, bit_order, mode, cs_pin), channel_(channel) {}

  void begin_transaction() override {
#ifdef USE_RP2
    SPISettings const settings(this->data_rate_, static_cast<BitOrder>(this->bit_order_), this->mode_);
#elif defined(ESP8266)
    // Arduino ESP8266 library has mangled values for SPI modes :-(
    auto mode = (this->mode_ & 0x01) + ((this->mode_ & 0x02) << 3);
    ESP_LOGVV(TAG, "8266 mangled SPI mode 0x%X", mode);
    SPISettings const settings(this->data_rate_, this->bit_order_, mode);
#else
    SPISettings const settings(this->data_rate_, this->bit_order_, this->mode_);
#endif
    this->channel_->beginTransaction(settings);
    SPIDelegate::begin_transaction();
  }

  void transfer(uint8_t *ptr, size_t length) override { this->channel_->transfer(ptr, length); }

  void end_transaction() override {
    this->channel_->endTransaction();
    SPIDelegate::end_transaction();
  }

  uint8_t transfer(uint8_t data) override { return this->channel_->transfer(data); }

  void write16(uint16_t data) override { this->channel_->transfer16(data); }

  void write_array(const uint8_t *ptr, size_t length) override {
    if (length == 1) {
      this->channel_->transfer(*ptr);
      return;
    }
#ifdef USE_RP2
    this->channel_->transfer(ptr, nullptr, length);
#elif defined(USE_ESP8266)
    // writeBytes() needs a word aligned pointer; transferBytes() bounces unaligned chunks through a stack buffer
    this->channel_->transferBytes(ptr, nullptr, length);
#else
    this->channel_->writeBytes(ptr, length);
#endif
  }

  void read_array(uint8_t *ptr, size_t length) override { this->channel_->transfer(ptr, length); }

 protected:
  SPIInterface channel_{};
};

bool SPIComponent::setup_hw_bus_() {
  SPIInterface channel = this->interface_;
  const int clk = Utility::get_pin_no(this->clk_pin_);
  const int sdi = Utility::get_pin_no(this->sdi_pin_);
  const int sdo = Utility::get_pin_no(this->sdo_pin_);
#ifdef USE_ESP8266
  channel->pins(clk, sdi, sdo, -1);
  channel->begin();
#endif  // USE_ESP8266
#ifdef USE_ESP32
  channel->begin(clk, sdi, sdo, -1);
#endif
#ifdef USE_RP2
  if (sdi != -1)
    channel->setRX(sdi);
  if (sdo != -1)
    channel->setTX(sdo);
  channel->setSCK(clk);
  channel->begin();
#endif
  return true;
}

SPIDelegate *SPIComponent::new_hw_delegate_(uint32_t data_rate, SPIBitOrder bit_order, SPIMode mode, GPIOPin *cs_pin,
                                            bool release_device, bool write_only) {
  return new SPIDelegateHw(this->interface_, data_rate, bit_order, mode, cs_pin);  // NOLINT
}

#endif  // USE_ARDUINO && !USE_ESP32
}  // namespace esphome::spi
