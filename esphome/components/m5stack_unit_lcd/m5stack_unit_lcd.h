#pragma once

#include <array>
#include <cstdint>

#include "esphome/components/display/display.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/split_buffer/split_buffer.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"

namespace esphome::m5stack_unit_lcd {

/// M5Stack Unit LCD (U120): a 135x240 ST7789V2 panel driven by an internal ESP32-PICO that
/// exposes a drawing command set over I2C (default address 0x3E).
///
/// Drawing happens in a local RGB565 frame buffer. On every update() the frame buffer is diffed
/// against a shadow copy of what the panel currently shows and only the changed row spans are
/// pushed with CASET/RASET + WRITE_RAW_16. The I2C link is slow (a full frame is ~65 KB, about
/// 3.5 s at 400 kHz with one chunk per main-loop tick), so the transfer is time-sliced from
/// loop() and the main loop keeps running while pixels stream out.
///
/// Memory: two 135x240x16-bit buffers, about 130 KB in total, allocated in PSRAM when available.
/// Rotation is applied in software; the panel stays in its native portrait orientation.
class M5StackUnitLCD : public display::Display, public i2c::I2CDevice {
 public:
  static constexpr int WIDTH = 135;
  static constexpr int HEIGHT = 240;
  static constexpr size_t BYTES_PER_PIXEL = 2;
  static constexpr size_t TX_HEADER_LEN = 7;  // CASET(3) + RASET(3) + WRITE_RAW_16(1)
#ifdef USE_ARDUINO
  /// Arduino Wire buffers a whole transaction: 128 bytes on ESP32, so keep each write under that.
  static constexpr size_t MAX_PIXELS_PER_TX = (128 - TX_HEADER_LEN) / BYTES_PER_PIXEL;
#else
  /// 263-byte transactions finish within ESP-IDF's 100 ms transaction timeout even at 50 kHz.
  static constexpr size_t MAX_PIXELS_PER_TX = 128;
#endif

  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;
  void on_powerdown() override { this->set_sleep(true); }
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }
  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }

  int get_width() override;
  int get_height() override;
  void draw_pixel_at(int x, int y, Color color) override;
  void fill(Color color) override;

  /// Backlight brightness, 0.0 to 1.0. Applied immediately once the display is running.
  void set_brightness(float brightness);
  /// Invert the panel colours.
  void set_invert_colors(bool invert);
  /// Put the panel to sleep (true) or wake it (false).
  void set_sleep(bool enable);

  /// True while changed pixels from the last update() are still being pushed to the panel.
  bool is_transfer_pending() const { return this->transfer_active_; }

 protected:
  static constexpr size_t ROW_BYTES = WIDTH * BYTES_PER_PIXEL;
  static constexpr size_t FRAME_BYTES = ROW_BYTES * HEIGHT;

  int get_width_internal() override { return WIDTH; }
  int get_height_internal() override { return HEIGHT; }

  void set_native_pixel_(int x, int y, uint16_t rgb565);
  bool row_touched_(int y) const { return (this->touched_rows_[y >> 3] >> (y & 7)) & 1; }
  void mark_row_touched_(int y) { this->touched_rows_[y >> 3] |= 1 << (y & 7); }
  void clear_row_touched_(int y) { this->touched_rows_[y >> 3] &= ~(1 << (y & 7)); }

  bool read_id_();
  bool send_(const uint8_t *data, size_t len);
  bool send_config_();
  /// Reserve room in the unit's command buffer for a command of the given cost. Non-blocking:
  /// returns false if the panel is still busy and the caller should retry on a later loop().
  bool reserve_(int cost);
  /// Compare a row against the shadow buffer; on a difference return the changed column span.
  bool row_dirty_(int y, int &x0, int &x1);
  /// Find the next run of dirty rows starting at next_row_ and make it the active region.
  bool find_next_region_();
  /// Send up to MAX_PIXELS_PER_TX pixels of the active region in one I2C transaction.
  bool send_next_chunk_();
  void finish_transfer_();

  split_buffer::SplitBuffer buffer_;  // what the lambda draws
  split_buffer::SplitBuffer prev_;    // what the panel shows
  /// One bit per row, set when a pixel write changed that row. Cleared only once a scan has
  /// found the row identical to the shadow copy, so nothing sent while the frame was still
  /// being drawn can be mistaken for up to date.
  std::array<uint8_t, (HEIGHT + 7) / 8> touched_rows_{};
  uint8_t tx_[TX_HEADER_LEN + MAX_PIXELS_PER_TX * BYTES_PER_PIXEL];

  // Transfer state machine, driven from loop() while transfer_active_.
  bool transfer_active_{false};
  bool rescan_{false};  // update() ran mid-transfer: scan the frame again when this pass ends
  int next_row_{0};
  bool region_active_{false};
  bool region_started_{false};  // window (CASET/RASET) already sent for the active region
  int region_xs_{0};
  int region_xe_{0};
  int region_y_{0};
  int region_y_end_{0};
  int region_x_{0};
  uint32_t transfer_start_{0};
  size_t transfer_pixels_{0};
  size_t transfer_tx_{0};

  // Command-buffer credit tracking (see reserve_()).
  int buf_free_{0};
  bool waiting_for_credit_{false};
  uint32_t credit_wait_start_{0};

  uint8_t brightness_{255};
  bool invert_{false};
  uint8_t fw_major_{0};
  uint8_t fw_minor_{0};
};

}  // namespace esphome::m5stack_unit_lcd
