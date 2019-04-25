#include "spi.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/application.h"

namespace esphome {
namespace spi {

static const char *TAG = "spi";

void ICACHE_RAM_ATTR HOT SPIComponent::write_byte(uint8_t data) {
  uint8_t send_bits = data;
  if (this->msb_first_)
    send_bits = reverse_bits_8(data);

  this->clk_->digital_write(true);
  if (!this->high_speed_)
    delayMicroseconds(5);

  for (size_t i = 0; i < 8; i++) {
    if (!this->high_speed_)
      delayMicroseconds(5);
    this->clk_->digital_write(false);

    // sampling on leading edge
    this->mosi_->digital_write(send_bits & (1 << i));
    if (!this->high_speed_)
      delayMicroseconds(5);
    this->clk_->digital_write(true);
  }

  ESP_LOGVV(TAG, "    Wrote 0b" BYTE_TO_BINARY_PATTERN " (0x%02X)", BYTE_TO_BINARY(data), data);
}

uint8_t ICACHE_RAM_ATTR HOT SPIComponent::read_byte() {
  this->clk_->digital_write(true);

  uint8_t data = 0;
  for (size_t i = 0; i < 8; i++) {
    if (!this->high_speed_)
      delayMicroseconds(5);
    data |= uint8_t(this->miso_->digital_read()) << i;
    this->clk_->digital_write(false);
    if (!this->high_speed_)
      delayMicroseconds(5);
    this->clk_->digital_write(true);
  }

  if (this->msb_first_) {
    data = reverse_bits_8(data);
  }

  ESP_LOGVV(TAG, "    Received 0b" BYTE_TO_BINARY_PATTERN " (0x%02X)", BYTE_TO_BINARY(data), data);

  return data;
}
void ICACHE_RAM_ATTR HOT SPIComponent::read_array(uint8_t *data, size_t length) {
  for (size_t i = 0; i < length; i++)
    data[i] = this->read_byte();
}

void ICACHE_RAM_ATTR HOT SPIComponent::write_array(uint8_t *data, size_t length) {
  for (size_t i = 0; i < length; i++) {
    App.feed_wdt();
    this->write_byte(data[i]);
  }
}

void ICACHE_RAM_ATTR HOT SPIComponent::enable(GPIOPin *cs, bool msb_first, bool high_speed) {
  ESP_LOGVV(TAG, "Enabling SPI Chip on pin %u...", cs->get_pin());
  cs->digital_write(false);

  this->active_cs_ = cs;
  this->msb_first_ = msb_first;
  this->high_speed_ = high_speed;
}

void ICACHE_RAM_ATTR HOT SPIComponent::disable() {
  ESP_LOGVV(TAG, "Disabling SPI Chip on pin %u...", this->active_cs_->get_pin());
  this->active_cs_->digital_write(true);
  this->active_cs_ = nullptr;
}
void SPIComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SPI bus...");
  this->clk_->setup();
  this->clk_->digital_write(true);
  if (this->miso_ != nullptr) {
    this->miso_->setup();
  }
  if (this->mosi_ != nullptr) {
    this->mosi_->setup();
    this->mosi_->digital_write(false);
  }
}
void SPIComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SPI bus:");
  LOG_PIN("  CLK Pin: ", this->clk_);
  LOG_PIN("  MISO Pin: ", this->miso_);
  LOG_PIN("  MOSI Pin: ", this->mosi_);
}
float SPIComponent::get_setup_priority() const { return setup_priority::BUS; }

}  // namespace spi
}  // namespace esphome
