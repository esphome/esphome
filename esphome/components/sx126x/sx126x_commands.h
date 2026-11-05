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
// Commands record failure in the SPI device: callers start a sequence with begin()
// and check ok() at its end, or before acting on a value that was read.
class Sx126xCommands {
 public:
  Sx126xCommands() = default;
  Sx126xCommands(const Sx126xCommands &) = delete;
  Sx126xCommands &operator=(const Sx126xCommands &) = delete;
  Sx126xSpiDevice &bus() { return this->spi_device_; }
  // Fills applied as it goes; only use it if the result is OK.
  [[nodiscard]] ConfigurationResult configure(const RadioProfile &profile, std::span<char, 16> version,
                                              AppliedProfile &applied);
  // Sets the payload length in the applied packet parameters and writes them.
  void set_packet_length(AppliedProfile &applied, uint8_t length);
  void calibrate_image(uint32_t frequency);
  void standby(SX126xStandbyMode mode);
  void receive();
  void transmit(uint32_t timeout_ms = 0);
  void sleep(bool cold = false);
  void clear_irqs(uint16_t mask = 0xFFFF);
  // Returns 0 if the read was skipped or failed.
  uint16_t read_irq();
  void write_fifo(const uint8_t *data, size_t size);
  // Leaves the packet unchanged if a read fails.
  void read_packet(uint8_t modulation, std::vector<uint8_t> &packet, float &rssi, float &snr);
  static uint32_t bandwidth_hz(SX126xBw bw);
  static uint16_t ramp_us(uint8_t ramp);

 private:
  void read_opcode_(uint8_t opcode, uint8_t *data, uint8_t size);
  void write_opcode_(uint8_t opcode, const uint8_t *data, uint8_t size);
  void read_register_(uint16_t reg, uint8_t *data, uint8_t size);
  void write_register_(uint16_t reg, const uint8_t *data, uint8_t size);
  static void encode_packet_params_(const RadioProfile &profile, AppliedProfile &applied);
  Sx126xSpiDevice spi_device_;
};
}  // namespace esphome::sx126x
