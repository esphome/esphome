#include "spi.h"
#include <vector>

namespace esphome {
namespace spi {

#ifdef USE_ARDUINO

static const char *const TAG = "spi-esp-arduino";
class SPIDelegateHw : public SPIDelegate {
 public:
  SPIDelegateHw(SPIInterface channel, uint32_t data_rate, SPIBitOrder bit_order, SPIMode mode, GPIOPin *cs_pin)
      : SPIDelegate(data_rate, bit_order, mode, cs_pin), channel_(channel) {}

  void begin_transaction() override {
#ifdef USE_RP2040
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

#ifdef USE_RP2040
  void write_array(const uint8_t *ptr, size_t length) override {
    // avoid overwriting the supplied buffer
    uint8_t *rxbuf = new uint8_t[length];  // NOLINT(cppcoreguidelines-owning-memory)
    memcpy(rxbuf, ptr, length);
    this->channel_->transfer((void *) rxbuf, length);
    delete[] rxbuf;  // NOLINT(cppcoreguidelines-owning-memory)
  }
#else
  void write_array(const uint8_t *ptr, size_t length) override { this->channel_->writeBytes(ptr, length); }
#endif

  void read_array(uint8_t *ptr, size_t length) override { this->channel_->transfer(ptr, length); }

 protected:
  SPIInterface channel_{};
};

class SPIBusHw : public SPIBus {
 public:
  SPIBusHw(GPIOPin *clk, GPIOPin *sdo, GPIOPin *sdi, SPIInterface channel) : SPIBus(clk, sdo, sdi), channel_(channel) {
#ifdef USE_ESP8266
    channel->pins(Utility::get_pin_no(clk), Utility::get_pin_no(sdi), Utility::get_pin_no(sdo), -1);
    channel->begin();
#endif  // USE_ESP8266
#ifdef USE_ESP32
    channel->begin(Utility::get_pin_no(clk), Utility::get_pin_no(sdi), Utility::get_pin_no(sdo), -1);
#endif
#ifdef USE_RP2040
    if (Utility::get_pin_no(sdi) != -1)
      channel->setRX(Utility::get_pin_no(sdi));
    if (Utility::get_pin_no(sdo) != -1)
      channel->setTX(Utility::get_pin_no(sdo));
    channel->setSCK(Utility::get_pin_no(clk));
    channel->begin();
#endif
  }

  SPIDelegate *get_delegate(uint32_t data_rate, SPIBitOrder bit_order, SPIMode mode, GPIOPin *cs_pin) override {
    return new SPIDelegateHw(this->channel_, data_rate, bit_order, mode, cs_pin);
  }

 protected:
  SPIInterface channel_{};
  bool is_hw() override { return true; }
};

SPIBus *SPIComponent::get_bus(SPIInterface interface, GPIOPin *clk, GPIOPin *sdo, GPIOPin *sdi) {
  return new SPIBusHw(clk, sdo, sdi, interface);
}

#endif  // USE_ARDUINO
}  // namespace spi
}  // namespace esphome
