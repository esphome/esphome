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
    SPI2_HOST,
#if !(defined(USE_ESP32_VARIANT_ESP32C3) || defined(USE_ESP32_VARIANT_ESP32C2) || defined(USE_ESP32_VARIANT_ESP32C6))
    SPI3_HOST
#endif  // VARIANTS
#endif  // USE_ESP32
};

class SPIDelegateHw : public SPIDelegate {
 public:
  SPIDelegateHw(spi_host_device_t channel, uint32_t data_rate, SPIBitOrder bit_order, SPIMode mode, GPIOPin *cs_pin,
                bool write_only)
      : SPIDelegate(data_rate, bit_order, mode, cs_pin), channel_(channel), write_only_(write_only) {

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
    if (write_only)
      config.flags |= SPI_DEVICE_HALFDUPLEX | SPI_DEVICE_NO_DUMMY;
    esp_err_t const err = spi_bus_add_device(channel, &config, &this->handle_);
    if (err != ESP_OK)
      ESP_LOGE(TAG, "Add device failed - err %X", err);
  }

  bool is_ready() override { return this->handle_ != nullptr; }

  void begin_transaction() override {
    if (this->is_ready()) {
      if (spi_device_acquire_bus(this->handle_, portMAX_DELAY) != ESP_OK)
        ESP_LOGE(TAG, "Failed to acquire SPI bus");
      SPIDelegate::begin_transaction();
    } else {
      ESP_LOGW(TAG, "spi_setup called before initialisation");
    }
  }

  void end_transaction() override {
    if (this->is_ready()) {
      SPIDelegate::end_transaction();
      spi_device_release_bus(this->handle_);
    }
  }

  ~SPIDelegateHw() override {
    esp_err_t const err = spi_bus_remove_device(this->handle_);
    if (err != ESP_OK)
      ESP_LOGE(TAG, "Remove device failed - err %X", err);
  }

  // do a transfer. either txbuf or rxbuf (but not both) may be null.
  // transfers above the maximum size will be split.
  // TODO - make use of the queue for interrupt transfers to provide a (short) pipeline of blocks
  // when splitting is required.
  void transfer(const uint8_t *txbuf, uint8_t *rxbuf, size_t length) {
    if (rxbuf != nullptr && this->write_only_) {
      ESP_LOGE(TAG, "Attempted read from write-only channel");
      return;
    }
    spi_transaction_t desc = {};
    desc.flags = 0;
    while (length != 0) {
      size_t const partial = std::min(length, MAX_TRANSFER_SIZE);
      desc.length = txbuf != nullptr ? partial * 8 : 0;
      desc.rxlength = rxbuf != nullptr ? partial * 8 : 0;
      desc.tx_buffer = txbuf;
      desc.rx_buffer = rxbuf;
      esp_err_t const err = spi_device_transmit(this->handle_, &desc);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "Transmit failed - err %X", err);
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

  void write16(uint16_t data) override {
    if (this->bit_order_ == BIT_ORDER_MSB_FIRST) {
      uint16_t txbuf = SPI_SWAP_DATA_TX(data, 16);
      this->transfer((uint8_t *) &txbuf, nullptr, 2);
    } else {
      this->transfer((uint8_t *) &data, nullptr, 2);
    }
  }

  void write_array(const uint8_t *ptr, size_t length) override { this->transfer(ptr, nullptr, length); }

  void write_array16(const uint16_t *data, size_t length) override {
    if (this->bit_order_ == BIT_ORDER_LSB_FIRST) {
      this->write_array((uint8_t *) data, length * 2);
    } else {
      uint16_t buffer[MAX_TRANSFER_SIZE / 2];
      while (length != 0) {
        size_t const partial = std::min(length, MAX_TRANSFER_SIZE / 2);
        for (size_t i = 0; i != partial; i++) {
          buffer[i] = SPI_SWAP_DATA_TX(*data++, 16);
        }
        this->write_array((const uint8_t *) buffer, partial * 2);
        length -= partial;
      }
    }
  }

  void read_array(uint8_t *ptr, size_t length) override { this->transfer(nullptr, ptr, length); }

 protected:
  spi_host_device_t channel_{};
  spi_device_handle_t handle_{};
  bool write_only_{false};
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
      ESP_LOGE(TAG, "Bus init failed - err %X", err);
  }

  SPIDelegate *get_delegate(uint32_t data_rate, SPIBitOrder bit_order, SPIMode mode, GPIOPin *cs_pin) override {
    return new SPIDelegateHw(this->channel_, data_rate, bit_order, mode, cs_pin,
                             Utility::get_pin_no(this->sdi_pin_) == -1);
  }

 protected:
  spi_host_device_t channel_{};

  bool is_hw() override { return true; }
};

SPIBus *SPIComponent::get_bus(int interface, GPIOPin *clk, GPIOPin *sdo, GPIOPin *sdi) {
  spi_host_device_t channel;

  ESP_LOGD(TAG, "Initialise bus %d", interface);
  if ((size_t) interface >= bus_list.size()) {
    ESP_LOGE(TAG, "Invalid interface %d", interface);
    return nullptr;
  }
  channel = bus_list[interface];
  return new SPIBusHw(clk, sdo, sdi, channel);
}

#endif
}  // namespace spi
}  // namespace esphome
