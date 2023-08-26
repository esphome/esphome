#include "spi.h"
#include "esphome/core/log.h"

namespace esphome {
namespace spi {

#ifdef  USE_ARDUINO

class SPIDelegateHw : public SPIDelegate {
 public:
  SPIDelegateHw(SPIClass *channel, uint32_t data_rate, SPIBitOrder bit_order, SPIMode mode,
                GPIOPin *cs_pin) : SPIDelegate(data_rate, bit_order, mode, cs_pin), channel_(channel) {}

  void begin_transaction() override {
#ifdef USE_RP2040
    SPISettings settings(this->data_rate_, static_cast<BitOrder>(this->bit_order_), this->mode_);
#else
    SPISettings const settings(this->data_rate_, this->bit_order_, this->mode_);
#endif
    SPIDelegate::begin_transaction();
    this->channel_->beginTransaction(settings);
  }

  void transfer(uint8_t *ptr, size_t length) override {
    this->channel_->transfer(ptr, length);
  }

  void end_transaction() override {
    this->channel_->endTransaction();
    SPIDelegate::end_transaction();
  }

  uint8_t transfer(uint8_t data) override { return this->channel_->transfer(data); }

  uint16_t transfer16(uint16_t data) override { return this->channel_->transfer(data); }

  void write_array(const uint8_t *ptr, size_t length) override { this->channel_->writeBytes(ptr, length); }

  void read_array(uint8_t *ptr, size_t length) override { this->channel_->transfer(ptr, length); }

 protected:
  SPIClass *channel_{};
};


class SPIBusHw : public SPIBus {
 public:
  SPIBusHw(GPIOPin *clk, GPIOPin *sdo, GPIOPin *sdi, SPIClass *channel) : SPIBus(clk, sdo, sdi),
                                                                          channel_(channel) {

#ifdef USE_ESP8266
    channel->pins(Utility::get_pin_no(clk), Utility::get_pin_no(sdi), Utility::get_pin_no(sdo), -1);
    channel->begin();
#endif  // USE_ESP8266
#ifdef USE_ESP32
    channel->begin(Utility::get_pin_no(clk), Utility::get_pin_no(sdi), Utility::get_pin_no(sdo), -1);
#endif
#ifdef USE_RP2040
    if (get_pin_no(sdi) != -1)
      channel->setRX(get_pin_no(sdi));
    if (this->get_pin_no(sdo_pin) != -1)
      channel->setTX(this->get_pin_no(sdo_pin));
    channel->setSCK(get_pin_no(clk_pin));
    channel->begin();
#endif
  }

  SPIDelegate *get_delegate(uint32_t data_rate, SPIBitOrder bit_order, SPIMode mode, GPIOPin *cs_pin) override {
    return new SPIDelegateHw(this->channel_, data_rate, bit_order, mode, cs_pin);
  }

 protected:
  SPIClass *channel_{};
};

SPIBus *SPIComponent::get_next_bus(unsigned int num, GPIOPin *clk, GPIOPin *sdo, GPIOPin *sdi) {
  SPIClass *channel;

#ifdef USE_ESP8266
  channel = &SPI;
#endif  // USE_ESP8266
#ifdef USE_ESP32
  if (num == 0) {
    channel = &SPI;
  } else {
#if defined(USE_ESP32_VARIANT_ESP32C3) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || \
    defined(USE_ESP32_VARIANT_ESP32C2) || defined(USE_ESP32_VARIANT_ESP32C6)
    channel = new SPIClass(FSPI);  // NOLINT(cppcoreguidelines-owning-memory)
#else
    channel = new SPIClass(HSPI);  // NOLINT(cppcoreguidelines-owning-memory)
#endif  // USE_ESP32_VARIANT
  }
#endif  // USE_ESP32
  return new SPIBusHw(clk, sdo, sdi, channel);
}

#endif  //USE_ARDUINO
}  // namespace spi
}  // namespace esphome
