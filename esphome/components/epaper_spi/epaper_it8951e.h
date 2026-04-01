#pragma once

#include "epaper_spi.h"
#include "epaper_it8951e_defs.h"

namespace esphome::epaper_spi {

class EPaperIT8951E : public EPaperBase {
 public:
  EPaperIT8951E(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                size_t init_sequence_length)
      : EPaperBase(name, width, height, init_sequence, init_sequence_length,
                   display::DisplayType::DISPLAY_TYPE_GRAYSCALE) {
    // IT8951E uses 4 bits per pixel (2 pixels per byte)
    this->row_width_ = static_cast<uint16_t>((static_cast<uint32_t>(width) + 1) / 2);
    this->buffer_length_ = static_cast<size_t>(this->row_width_) * static_cast<size_t>(height);
  }

  void set_reversed(bool reversed) { this->reversed_ = reversed; }
  void set_sleep_when_done(bool sleep_when_done) { this->sleep_when_done_ = sleep_when_done; }

  // Component overrides
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

  // Additional update modes
  void update_fast();

  // Drawing overrides
  void fill(Color color) override;
  void draw_pixel_at(int x, int y, Color color) override;

 protected:
  // EPaperBase required overrides
  bool reset() override;
  bool transfer_data() override;
  void refresh_screen(bool partial) override;
  void power_on() override;
  void power_off() override;
  void deep_sleep() override;

 private:
  // IT8951E SPI protocol methods
  void write_two_byte16_(uint16_t type, uint16_t cmd);
  uint16_t read_word_();
  void write_command_(uint16_t cmd);
  void write_word_(uint16_t cmd);
  void write_reg_(uint16_t addr, uint16_t data);
  void set_target_memory_addr_(uint16_t tar_addr_l, uint16_t tar_addr_h);
  void write_args_(uint16_t cmd, uint16_t *args, uint16_t length);

  // Display area management
  void set_area_(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
  void update_area_(uint16_t x, uint16_t y, uint16_t w, uint16_t h, update_mode_e mode);

  // Busy/idle management (IT8951E busy pin polarity: HIGH = ready, LOW = busy)
  void wait_busy_(uint32_t timeout = 1000);
  bool is_display_busy_();

  // VCOM management
  uint16_t get_vcom_();
  void set_vcom_(uint16_t vcom);

  // Transfer helpers
  bool prepare_transfer_(update_mode_e &mode);
  bool transfer_row_data_();
  void process_state_();
  void set_state_(EPaperState state, uint16_t delay = 0);

  // Color conversion
  uint8_t color_to_nibble_(const Color &color) const;

  // IT8951E device info
  uint16_t usImgBufAddrL_{0x36E0};
  uint16_t usImgBufAddrH_{0x0012};
  char usFWVersion_[16]{};
  char usLUTVersion_[16]{};
  uint16_t m_endian_type_{0};
  uint16_t m_pix_bpp_{0};

  // Configuration
  bool reversed_{false};
  bool sleep_when_done_{true};

  // State tracking
  bool initialized_{false};
  uint32_t partial_update_{0};
  update_mode_e pending_mode_{UPDATE_MODE_NONE};
  update_mode_e queued_update_mode_{UPDATE_MODE_NONE};
  uint16_t pending_x_{0}, pending_y_{0}, pending_w_{0}, pending_h_{0};
  uint16_t transfer_row_{0};
  uint32_t update_started_at_{0};
  bool update_timing_active_{false};
  bool update_pending_{false};
};

}  // namespace esphome::epaper_spi
