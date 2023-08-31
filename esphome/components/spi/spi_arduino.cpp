#include "spi.h"
#include <vector>

namespace esphome {
namespace spi {

#ifdef USE_ARDUINO

static const char *const TAG = "spi-esp-arduino";
#ifdef USE_RP2040
using SPIBusDelegate = SPIClassRP2040;
#else
using SPIBusDelegate = SPIClass;
#endif

// list of available buses
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables, cppcoreguidelines-interfaces-global-init)
static std::vector<std::function<SPIBusDelegate *()>> bus_list = {
#ifdef USE_ESP32
    [] { return &SPI; },  // NOLINT(cppcoreguidelines-interfaces-global-init)
#if defined(USE_ESP32_VARIANT_ESP32C3) || defined(USE_ESP32_VARIANT_ESP32C2) || defined(USE_ESP32_VARIANT_ESP32C6)
#elif defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
    [] { return new SPIClass(FSPI); },  // NOLINT(cppcoreguidelines-owning-memory)
#else
    [] { return new SPIClass(HSPI); },  // NOLINT(cppcoreguidelines-owning-memory)
#endif  // USE_ESP32_VARIANT
#endif  // USE_ESP32
#ifdef USE_RP2040
// Doesn't seem to be defined.
//  [] { return &SPI1; },  // NOLINT(cppcoreguidelines-interfaces-global-init)
#endif
};

class SPIDelegateHw : public SPIDelegate {
 public:
  SPIDelegateHw(SPIBusDelegate *channel, uint32_t data_rate, SPIBitOrder bit_order, SPIMode mode, GPIOPin *cs_pin)
      : SPIDelegate(data_rate, bit_order, mode, cs_pin), channel_(channel) {}

  void begin_transaction() override {
#ifdef USE_RP2040
    SPISettings const settings(this->data_rate_, static_cast<BitOrder>(this->bit_order_), this->mode_);
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

  uint16_t transfer16(uint16_t data) override { return this->channel_->transfer16(data); }

#ifdef USE_RP2040
  void write_array(const uint8_t *ptr, size_t length) override {
    uint8_t *rxbuf = new uint8_t[length];
    memcpy(rxbuf, ptr, length);
    this->channel_->transfer((void *) rxbuf, length);
  }
#else
  void write_array(const uint8_t *ptr, size_t length) override { this->channel_->writeBytes(ptr, length); }
#endif

  void read_array(uint8_t *ptr, size_t length) override { this->channel_->transfer(ptr, length); }

 protected:
  SPIBusDelegate *channel_{};
};

class SPIBusHw : public SPIBus {
 public:
  SPIBusHw(GPIOPin *clk, GPIOPin *sdo, GPIOPin *sdi, SPIBusDelegate *channel)
      : SPIBus(clk, sdo, sdi), channel_(channel) {
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
  SPIBusDelegate *channel_{};
  bool is_hw() override { return true; }
};

SPIBus *SPIComponent::get_bus(int interface, GPIOPin *clk, GPIOPin *sdo, GPIOPin *sdi) {
  SPIBusDelegate *channel;

  if ((size_t) interface >= bus_list.size()) {
    ESP_LOGE(TAG, "Invalid interface %d", interface);
    return nullptr;
  }
  channel = bus_list[interface]();
  return new SPIBusHw(clk, sdo, sdi, channel);
}

#endif  // USE_ARDUINO
}  // namespace spi
}  // namespace esphome
