#pragma once

#include "esphome/components/spi/spi.h"
#include <span>

namespace esphome::sx126x {

// Persistent radio device. read()/write() use exchange_() for one complete selected interval.
// No raw SPI access escapes. Call only from the main loop; spans are borrowed until return.
class Sx126xSpiDevice {
 public:
  Sx126xSpiDevice() = default;
  Sx126xSpiDevice(const Sx126xSpiDevice &) = delete;
  Sx126xSpiDevice &operator=(const Sx126xSpiDevice &) = delete;
  void set_spi_parent(spi::SPIComponent *parent) { this->device_.set_spi_parent(parent); }
  void set_cs_pin(GPIOPin *pin) { this->device_.set_cs_pin(pin); }
  void set_data_rate(uint32_t rate) { this->device_.set_data_rate(rate); }
  void set_mode(spi::SPIMode mode) { this->device_.set_mode(mode); }
  void set_bit_order(spi::SPIBitOrder order) { this->device_.set_bit_order(order); }
  void set_release_device(bool release) { this->device_.set_release_device(release); }
  void set_write_only(bool write_only) { this->device_.set_write_only(write_only); }
#ifdef USE_SPI_PSRAM_DMA
  void set_psram_dma(bool enable) { this->device_.set_psram_dma(enable); }
#endif

  void set_busy_pin(GPIOPin *pin) { this->busy_pin_ = pin; }
  void setup() { this->device_.spi_setup(); }
  void teardown() { this->device_.spi_teardown(); }
  [[nodiscard]] bool write(std::span<const uint8_t> header, std::span<const uint8_t> data);
  [[nodiscard]] bool read(std::span<const uint8_t> header, std::span<uint8_t> data);

 private:
  using Device =
      spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_8MHZ>;
  class Selection {
   public:
    explicit Selection(Device &device) : device_(device) { this->device_.enable(); }
    ~Selection() { this->device_.disable(); }
    Selection(const Selection &) = delete;
    Selection &operator=(const Selection &) = delete;

   private:
    Device &device_;
  };
  [[nodiscard]] bool exchange_(std::span<const uint8_t> header, std::span<const uint8_t> tx, std::span<uint8_t> rx);
  Device device_;
  GPIOPin *busy_pin_{nullptr};
};

}  // namespace esphome::sx126x
