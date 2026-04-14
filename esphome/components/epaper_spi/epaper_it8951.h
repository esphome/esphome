#pragma once

#include "epaper_spi.h"
#include "epaper_it8951_defs.h"

namespace esphome::epaper_spi {

class EPaperIT8951 : public EPaperBase {
 public:
  EPaperIT8951(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
               size_t init_sequence_length)
      : EPaperBase(name, width, height, init_sequence, init_sequence_length,
                   display::DisplayType::DISPLAY_TYPE_GRAYSCALE) {
    // IT8951 uses 4 bits per pixel (2 pixels per byte)
    this->row_width_ = static_cast<uint16_t>((static_cast<uint32_t>(width) + 1) / 2);
    this->buffer_length_ = static_cast<size_t>(this->row_width_) * static_cast<size_t>(height);
  }

  // Component overrides
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

  /// Named update modes: "GC16" (default/full), "DU" (fast), "GL16", "GLR16", "GLD16", "DU4", "A2", "INIT".
  void update_mode(const std::string &mode) override;

  void set_force_1bpp(bool force_1bpp) { this->force_1bpp_ = force_1bpp; }

  // Drawing overrides
  void fill(Color color) override;
  void clear() override { this->fill(COLOR_OFF); }
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
  // IT8951 SPI protocol methods
  void write_two_byte16_(uint16_t type, uint16_t cmd, uint32_t timeout = 1000);
  uint16_t read_word_(uint32_t timeout = 1000);
  void read_words_(uint16_t *buf, uint32_t word_count);
  void write_command_(uint16_t cmd, uint32_t timeout = 1000);
  void write_word_(uint16_t data, uint32_t timeout = 1000);
  void write_reg_(uint16_t addr, uint16_t data);
  void set_target_memory_addr_(uint16_t tar_addr_l, uint16_t tar_addr_h);
  void write_args_(uint16_t cmd, const uint16_t *args, uint16_t length);

  // Display area management
  void set_area_(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
  void update_area_(uint16_t x, uint16_t y, uint16_t w, uint16_t h, UpdateModeE mode);
  void update_area_1bpp_(uint16_t x, uint16_t y, uint16_t w, uint16_t h, UpdateModeE mode, uint8_t bg_gray,
                         uint8_t fg_gray);
  void restore_1bpp_mode_();

  // Busy/idle management (IT8951 busy pin polarity: HIGH = ready, LOW = busy)
  void wait_busy_(uint32_t timeout = 1000);
  bool is_display_busy_();

  // VCOM management
  uint16_t read_vcom_();
  void write_vcom_(uint16_t vcom);

  // Device info
  void get_dev_info_();

  // Transfer helpers
  bool prepare_transfer_(UpdateModeE &mode);
  bool transfer_row_data_();
  bool transfer_row_data_1bpp_();
  void start_update_(UpdateModeE hw_mode);
  void process_state_();
  void set_state_(EPaperState state, uint16_t delay = 0);

  // 1bpp helpers
  bool framebuffer_is_binary_();
  uint8_t get_pixel_nibble_(uint16_t x, uint16_t y);

  // Color conversion
  uint8_t color_to_nibble_(const Color &color) const;

  // IT8951 device info
  IT8951DevInfo dev_info_{};
  uint16_t us_img_buf_addr_l_{0};  // Set from DevInfo during setup()
  uint16_t us_img_buf_addr_h_{0};  // Set from DevInfo during setup()
  uint16_t m_endian_type_{0};
  uint16_t m_pix_bpp_{0};

  // Recovery
  static constexpr uint32_t BUSY_TIMEOUT_MS = 5000;  // 5s timeout for busy waits in loop()
  void recover_();

  // Mode flags
  bool use_1bpp_{false};
  bool force_1bpp_{false};

  // State tracking
  bool initialized_{false};
  uint32_t partial_update_{0};
  UpdateModeE pending_mode_{UPDATE_MODE_NONE};
  UpdateModeE queued_update_mode_{UPDATE_MODE_NONE};
  UpdateModeE pending_update_mode_{UPDATE_MODE_NONE};  // mode for queued pending update
  uint16_t pending_x_{0}, pending_y_{0}, pending_w_{0}, pending_h_{0};
  uint16_t transfer_row_{0};
  uint32_t update_started_at_{0};
  uint32_t busy_wait_start_{0};
  bool update_timing_active_{false};
  bool update_pending_{false};
  bool pending_1bpp_restore_{false};
};

}  // namespace esphome::epaper_spi
