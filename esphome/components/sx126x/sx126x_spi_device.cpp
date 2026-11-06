#include "sx126x_spi_device.h"
#include "esphome/core/hal.h"

namespace esphome::sx126x {

static constexpr uint32_t BUSY_TIMEOUT_MS = 20;
static constexpr uint32_t SWITCHING_DELAY_US = 1;

void Sx126xSpiDevice::exchange_(std::span<const uint8_t> header, std::span<const uint8_t> tx, std::span<uint8_t> rx) {
  if (!this->ok_) {
    return;
  }
  // Selecting first wakes a sleeping radio. The guard releases CS even on timeout.
  Selection selected(this->device_);
  const uint32_t start = millis();
  while (this->busy_pin_->digital_read()) {
    if (millis() - start > BUSY_TIMEOUT_MS) {
      this->ok_ = false;
      return;
    }
  }
  for (auto byte : header) {
    this->device_.transfer_byte(byte);
  }
  for (auto byte : tx) {
    this->device_.transfer_byte(byte);
  }
  for (auto &byte : rx) {
    byte = this->device_.transfer_byte(0);
  }
}

void Sx126xSpiDevice::write(std::span<const uint8_t> header, std::span<const uint8_t> data) {
  this->exchange_(header, data, {});
  if (this->ok_) {
    // This delay belongs after CS is released, not in the selected interval.
    delayMicroseconds(SWITCHING_DELAY_US);
  }
}

void Sx126xSpiDevice::read(std::span<const uint8_t> header, std::span<uint8_t> data) {
  this->exchange_(header, {}, data);
}

}  // namespace esphome::sx126x
