#include "spi.h"
#include <vector>

namespace esphome {
namespace spi {

#ifdef USE_ESP_IDF
static const char *const TAG = "spi-esp-idf";
static const size_t MAX_TRANSFER_SIZE = 4092;  // dictated by ESP-IDF API.

// list of available buses
// https://bugs.llvm.org/show_bug.cgi?id=48040
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static std::vector<spi_host_device_t> bus_list = {
#ifdef USE_ESP8266
    SPI1_HOST,
#endif
#ifdef USE_ESP32
    SPI3_HOST, SPI2_HOST
#endif
};

class SPIDelegateHw : public SPIDelegate {
 public:
  SPIDelegateHw(spi_host_device_t channel, uint32_t data_rate, SPIBitOrder bit_order, SPIMode mode, GPIOPin *cs_pin)
      : SPIDelegate(data_rate, bit_order, mode, cs_pin), channel_(channel) {
    spi_device_interface_config_t config = {};
    config.mode = static_cast<uint8_t>(mode);
    config.clock_speed_hz = static_cast<int>(data_rate);
    config.spics_io_num = -1;
    config.flags = 0;
    config.queue_size = 1;
    config.pre_cb = nullptr;
    config.post_cb = nullptr;
    if (bit_order == BIT_ORDER_LSB_FIRST)
      config.flags |= SPI_DEVICE_BIT_LSBFIRST;
    esp_err_t const err = spi_bus_add_device(channel, &config, &this->handle_);
    if (err != ESP_OK)
      ESP_LOGE(TAG, "Add device failed - err %d", err);
  }

  ~SPIDelegateHw() override {
    esp_err_t const err = spi_bus_remove_device(this->handle_);
    if (err != ESP_OK)
      ESP_LOGE(TAG, "Remove device failed - err %d", err);
  }

  // do a transfer. either txbuf or rxbuf (but not both) may be null.
  // transfers above the maximum size will be split.
  // TODO - make use of the queue for interrupt transfers to provide a (short) pipeline of blocks
  // when splitting is required.
  void transfer(const uint8_t *txbuf, uint8_t *rxbuf, size_t length) {
    spi_transaction_t desc = {};
    desc.flags = 0;
    while (length != 0) {
      size_t const partial = std::min(length, MAX_TRANSFER_SIZE);
      desc.length = partial * 8;
      desc.rxlength = partial * 8;
      desc.tx_buffer = txbuf;
      desc.rx_buffer = rxbuf;
      esp_err_t const err = spi_device_transmit(this->handle_, &desc);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "Transmit failed - err %d", err);
        break;
      }
      length -= partial;
      if (txbuf != nullptr)
        txbuf += partial;
      if (rxbuf != nullptr)
        rxbuf += partial;
    }
  }

  void transfer(uint8_t *ptr, size_t length) override { this->transfer(ptr, ptr, length); }

  uint8_t transfer(uint8_t data) override {
    uint8_t rxbuf;
    this->transfer(&data, &rxbuf, 1);
    return rxbuf;
  }

  uint16_t transfer16(uint16_t data) override {
    uint16_t rxbuf;
    if (this->bit_order_ == BIT_ORDER_MSB_FIRST) {
      uint16_t txbuf = SPI_SWAP_DATA_TX(data, 16);
      this->transfer((uint8_t *) &txbuf, (uint8_t *) &rxbuf, 2);
      return SPI_SWAP_DATA_RX(rxbuf, 16);
    }
    this->transfer((uint8_t *) &data, (uint8_t *) &rxbuf, 2);
    return rxbuf;
  }

  void write_array(const uint8_t *ptr, size_t length) override { this->transfer(ptr, nullptr, length); }

  void read_array(uint8_t *ptr, size_t length) override { this->transfer(nullptr, ptr, length); }

 protected:
  spi_host_device_t channel_{};
  spi_device_handle_t handle_{};
};

class SPIBusHw : public SPIBus {
 public:
  SPIBusHw(GPIOPin *clk, GPIOPin *sdo, GPIOPin *sdi, spi_host_device_t channel)
      : SPIBus(clk, sdo, sdi), channel_(channel) {
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = Utility::get_pin_no(sdo);
    buscfg.miso_io_num = Utility::get_pin_no(sdi);
    buscfg.sclk_io_num = Utility::get_pin_no(clk);
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = MAX_TRANSFER_SIZE;
    auto err = spi_bus_initialize(channel, &buscfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK)
      ESP_LOGE(TAG, "Bus init failed - err %d", err);
  }

  SPIDelegate *get_delegate(uint32_t data_rate, SPIBitOrder bit_order, SPIMode mode, GPIOPin *cs_pin) override {
    return new SPIDelegateHw(this->channel_, data_rate, bit_order, mode, cs_pin);
  }

 protected:
  spi_host_device_t channel_{};

  bool is_hw() override { return true; }
};

SPIBus *SPIComponent::get_next_bus(GPIOPin *clk, GPIOPin *sdo, GPIOPin *sdi) {
  spi_host_device_t channel;

  if (bus_list.empty())
    return nullptr;
  channel = bus_list.back();
  bus_list.pop_back();
  return new SPIBusHw(clk, sdo, sdi, channel);
}

#endif
}  // namespace spi
}  // namespace esphome
