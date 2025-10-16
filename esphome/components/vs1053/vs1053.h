#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esphome/components/spi/spi.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/hal.h"
#include "vs1053_reg.h"

namespace esphome {
namespace vs1053 {

static constexpr const uint32_t VS1053_CANCEL_TIMEOUT_US = 1000000;  // 1 sec
static constexpr const uint32_t VS1053_LOOP_TIMEOUT_US = 8000;       // 8 ms

static constexpr const size_t VS1053_FIFO_LENGTH = 2048;
static constexpr const size_t VS1053_TRANSFER_SIZE = 32;  // DREQ must be checked at least every 32 bytes

static constexpr const size_t VS1053_FILL_LENGTH = 2052;
static constexpr const size_t VS1053_STOP_FILL_LENGTH = 2048;

static constexpr const uint8_t VS1053_VERSION = 4;  // Per datasheet, VS1053/VS8053 SS_VER = 4

// VS1053 has two SPI interfaces, a command (SCI) and data (SDI) interface
// XCS is the command select, XDCS is the data select
// Both interfaces are Mode 0 MSB First
class VS1053_SCI_SPIDevice : public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_200KHZ> {};
class VS1053_SDI_SPIDevice : public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_8MHZ> {};

class VS1053Component : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_reset_pin(GPIOPin* pin) { this->reset_pin_ = pin; }
  void set_dreq_pin(GPIOPin* pin) { this->dreq_pin_ = pin; }
  void set_sci_device(VS1053_SCI_SPIDevice* spi) { this->sci_spi_ = spi; }
  void set_sdi_device(VS1053_SDI_SPIDevice* spi) { this->sdi_spi_ = spi; }

  void set_volume(uint8_t left, uint8_t right);
  void cancel_playback();
  void play_file(const uint8_t* data, size_t length);

  void play_test_sine(uint16_t ms, uint32_t freq_hz = 1000, uint32_t sample_rate_hz = 44100);
  void play_test_sine_sdi(uint16_t ms);

 protected:
  GPIOPin* reset_pin_ = nullptr;
  GPIOPin* dreq_pin_ = nullptr;
  VS1053_SCI_SPIDevice* sci_spi_ = nullptr;
  VS1053_SDI_SPIDevice* sdi_spi_ = nullptr;

  enum class PlaybackState {
    Idle,
    Playing,
    Cancel,
    Cancelled,
    Stop,
    Error,
  };

  uint8_t volume_left_ = 200;
  uint8_t volume_right_ = 200;

  PlaybackState state_ = PlaybackState::Idle;
  const uint8_t* buffer_ = nullptr;
  const uint8_t* buffer_end_ = nullptr;
  uint8_t fill_buffer_[VS1053_TRANSFER_SIZE];
  size_t fill_remaining_ = 0;
  uint32_t cancel_start_ = 0;

  void finish_playback_();

  bool init_(bool soft_reset = false);
  bool get_cancel_bit_() const;
  void set_cancel_bit_() const;
  uint8_t get_fill_byte_() const;

  uint16_t get_parameter_(uint16_t addr) const;

  bool wait_data_ready_(uint32_t timeout_us) const;
  bool data_ready_() const { return this->dreq_pin_->digital_read(); }

  void data_write_(const uint8_t* buffer, size_t length) const;
  uint16_t command_transfer_(uint8_t instruction, uint8_t addr, uint16_t data) const;
  void command_write_(uint8_t addr, uint16_t data) const { this->command_transfer_(SCI_CMD_WRITE, addr, data); }
  uint16_t command_read_(uint8_t addr) const { return this->command_transfer_(SCI_CMD_READ, addr, 0); }
};

}  // namespace vs1053
}  // namespace esphome
