#pragma once
#include "radio_profile.h"
#include "sx126x_spi_device.h"
#include <vector>

namespace esphome::sx126x {
enum class ConfigurationResult {
  CONFIGURATION_RESULT_OK,
  CONFIGURATION_RESULT_BUSY_TIMEOUT,
  CONFIGURATION_RESULT_UNSUPPORTED
};

// Encodes chip commands. No ESPHome lifecycle, callbacks, or raw SPI access.
// Results describe command exchange; TX completion is a separate IRQ operation.
class Sx126xCommands {
 public:
  explicit Sx126xCommands(Sx126xSpiDevice &spi_device) : spi_device_(spi_device) {}
  [[nodiscard]] ConfigurationResult configure(const RadioProfile &profile, std::span<char, 16> version);
  [[nodiscard]] bool set_packet_params(const RadioProfile &profile, uint8_t length);
  [[nodiscard]] bool calibrate_image(uint32_t frequency);
  [[nodiscard]] bool standby(SX126xStandbyMode mode);
  [[nodiscard]] bool receive();
  [[nodiscard]] bool transmit(uint32_t timeout_ms = 0);
  [[nodiscard]] bool sleep(bool cold = false);
  [[nodiscard]] bool clear_irqs(uint16_t mask = 0xFFFF);
  [[nodiscard]] bool read_irq(uint16_t &status);
  [[nodiscard]] bool write_fifo(const uint8_t *data, size_t size);
  [[nodiscard]] bool read_packet(const RadioProfile &profile, std::vector<uint8_t> &packet, float &rssi, float &snr);
  static uint32_t bandwidth_hz(SX126xBw bw);
  static uint16_t ramp_us(uint8_t ramp);

 private:
  [[nodiscard]] bool read_fifo_(uint8_t offset, std::vector<uint8_t> &packet);
  [[nodiscard]] bool write_fifo_(uint8_t offset, const uint8_t *data, size_t len);
  [[nodiscard]] bool read_opcode_(uint8_t opcode, uint8_t *data, uint8_t size);
  [[nodiscard]] bool write_opcode_(uint8_t opcode, const uint8_t *data, uint8_t size);
  [[nodiscard]] bool read_register_(uint16_t reg, uint8_t *data, uint8_t size);
  [[nodiscard]] bool write_register_(uint16_t reg, const uint8_t *data, uint8_t size);
  Sx126xSpiDevice &spi_device_;
};
}  // namespace esphome::sx126x
