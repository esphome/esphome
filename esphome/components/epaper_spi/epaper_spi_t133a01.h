#pragma once

#include "epaper_spi.h"

namespace esphome::epaper_spi {

class EPaperT133A01 : public EPaperBase {
 public:
  EPaperT133A01(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                size_t init_sequence_length)
      : EPaperBase(name, width, height, init_sequence, init_sequence_length, DISPLAY_TYPE_COLOR) {
    this->buffer_length_ = width * height / 2;  // 2 pixels per byte
  }

  void add_enable_pin(GPIOPin *enable_pin) {
    if (this->enable_pins_count_ < MAX_ENABLE_PINS) {
      this->enable_pins_[this->enable_pins_count_++] = enable_pin;
    }
  }

  void set_cs1_pin(GPIOPin *cs1_pin) { this->cs1_pin_ = cs1_pin; }

  void setup() override;

  void dump_config() override;

  void fill(Color color) override;
  void clear() override;

 protected:
  bool reset() override;
  bool initialise(bool partial) override;

  bool transfer_data() override;
  void refresh_screen(bool partial) override;
  void power_on() override;
  void power_off() override;
  void deep_sleep() override;

  void draw_pixel_at(int x, int y, Color color) override;

  void cs1_command_(uint8_t value);
  void cs1_cmd_data_(uint8_t command, const uint8_t *data, size_t length);
  void send_init_sequence_dual_(const uint8_t *sequence, size_t length);
  void wait_for_idle_with_timeout_(uint32_t timeout_ms, const char *label) const;

  GPIOPin *cs1_pin_{};
  spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_2MHZ>
      cs1_device_{};

  static constexpr uint8_t MAX_ENABLE_PINS = 4;
  GPIOPin *enable_pins_[MAX_ENABLE_PINS]{};
  uint8_t enable_pins_count_{0};

  // Transfer state (T133A01 uses two controllers/chip-selects; each row is pushed in 2 halves)
  size_t transfer_index_{0};
  bool transfer_on_cs1_{false};
  bool transfer_dtm_sent_{false};
  bool transfer_prologue_done_{false};
  bool transfer_streaming_{false};

  // Transfer prologue sequencing to avoid blocking waits.
  uint8_t transfer_prologue_phase_{0};
};

}  // namespace esphome::epaper_spi
