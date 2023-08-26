#include "spi.h"
#include "esphome/core/log.h"

namespace esphome {
namespace spi {

#ifdef USE_ESP_IDF
const char *const TAG = "spi-esp-idf";

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
    esp_err_t const err = spi_bus_add_device(channel, &config, &this->handle_);
    if (err != ESP_OK)
      ESP_LOGE(TAG, "Add device failed - err %d", err);
  }

  void transfer(const void *txbuf, void *rxbuf, size_t length) {
    spi_transaction_t desc = {};
    desc.flags = 0;
    desc.length = length * 8;
    desc.rxlength = length * 8;
    desc.tx_buffer = txbuf;
    desc.rx_buffer = rxbuf;
    esp_err_t const err = spi_device_transmit(this->handle_, &desc);
    if (err != ESP_OK)
      ESP_LOGE(TAG, "Transmit failed - err %d", err);
  }

  void transfer(uint8_t *ptr, size_t length) override { this->transfer(ptr, ptr, length); }

  uint8_t transfer(uint8_t data) override {
    uint8_t rxbuf;
    this->transfer(&data, &rxbuf, 1);
    return rxbuf;
  }

  uint16_t transfer16(uint16_t data) override {
    uint16_t rxbuf;
    uint16_t txbuf = SPI_SWAP_DATA_TX(data, 16);
    this->transfer(&txbuf, &rxbuf, 2);
    return SPI_SWAP_DATA_RX(rxbuf, 16);
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
    buscfg.max_transfer_sz = 32;
    auto err = spi_bus_initialize(channel, &buscfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK)
      ESP_LOGE(TAG, "Bus init failed - err %d", err);
  }

  SPIDelegate *get_delegate(uint32_t data_rate, SPIBitOrder bit_order, SPIMode mode, GPIOPin *cs_pin) override {
    return new SPIDelegateHw(this->channel_, data_rate, bit_order, mode, cs_pin);
  }

 protected:
  spi_host_device_t channel_{};
};

SPIBus *SPIComponent::get_next_bus(unsigned int num, GPIOPin *clk, GPIOPin *sdo, GPIOPin *sdi) {
  spi_host_device_t channel;

#ifdef USE_ESP8266
  channel = SPI1_HOST;
#endif  // USE_ESP8266
#ifdef USE_ESP32
  if (num == 0) {
    channel = SPI2_HOST;
  } else
    channel = SPI3_HOST;
#endif  // USE_ESP32
  return new SPIBusHw(clk, sdo, sdi, channel);
}

#endif
}  // namespace spi
}  // namespace esphome
