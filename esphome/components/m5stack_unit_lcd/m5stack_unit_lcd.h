#pragma once

#include <algorithm>
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
/// If the unit stops answering (for example it was unplugged), the driver stops retrying and
/// checks for it again at each update. A unit that comes back has rebooted and lost its picture
/// and settings, so the settings are sent again and the whole frame is redrawn.
///
/// Memory: two 135x240x16-bit buffers, about 130 KB in total, allocated in PSRAM when available.
/// Rotation is applied in software; the panel stays in its native portrait orientation.
class M5StackUnitLCD : public display::Display, public i2c::I2CDevice {
 public:
  static constexpr int WIDTH = 135;
  static constexpr int HEIGHT = 240;
  static constexpr size_t BYTES_PER_PIXEL = 2;
  static constexpr size_t TX_HEADER_LEN = 7;  // CASET(3) + RASET(3) + WRITE_RAW_16(1)
#if defined(USE_ESP8266)
  /// The ESP8266 Wire library sends each write from a 128-byte buffer.
  static constexpr size_t MAX_PIXELS_PER_TX = (128 - TX_HEADER_LEN) / BYTES_PER_PIXEL;
#elif defined(USE_RP2)
  /// The RP2040 Wire library sends each write from a 256-byte buffer.
  static constexpr size_t MAX_PIXELS_PER_TX = (256 - TX_HEADER_LEN) / BYTES_PER_PIXEL;
#else
  /// The other I2C buses have no write buffer limit. A 263-byte write also finishes well inside
  /// ESP-IDF's 100 ms transaction timeout at the default 50 kHz.
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

  enum class CreditResult : uint8_t {
    CREDIT_RESULT_GRANTED,
    CREDIT_RESULT_WAIT,
    CREDIT_RESULT_BUS_ERROR,
  };

  int get_width_internal() override { return WIDTH; }
  int get_height_internal() override { return HEIGHT; }

  void set_native_pixel_(int x, int y, uint16_t rgb565);
  bool row_touched_(int y) const { return (this->touched_rows_[y >> 3] >> (y & 7)) & 1; }
  void mark_row_touched_(int y) { this->touched_rows_[y >> 3] |= 1 << (y & 7); }
  void clear_row_touched_(int y) { this->touched_rows_[y >> 3] &= ~(1 << (y & 7)); }
  /// Copy len bytes of the frame buffer starting at index into dst.
  void read_frame_(size_t index, uint8_t *dst, size_t len);
  /// Copy len bytes of the frame buffer starting at index into the shadow copy.
  void mirror_frame_(size_t index, size_t len);

  bool read_id_();
  bool send_(const uint8_t *data, size_t len);
  /// Send a single command outside a transfer. When the unit is not reachable the state is only
  /// stored; send_config_() applies it after the next (re)connect.
  void send_command_(const uint8_t *data, size_t len);
  bool send_config_();
  /// Probe the unit before a transfer. Returns false when it is not reachable.
  bool check_connection_();
  void start_transfer_();
  void note_bus_error_();
  void note_bus_ok_();
  void lose_connection_();
  /// Reserve room in the unit's command buffer for a chunk of the given cost. Non-blocking: on
  /// CREDIT_RESULT_WAIT the caller retries on a later loop().
  CreditResult reserve_(int cost);
  /// Account for a one-off command without querying the unit or touching the wait timer.
  void consume_credit_(int cost) { this->buf_free_ = std::max(0, this->buf_free_ - cost); }
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
  bool rescan_{false};       // update() ran mid-transfer: scan the frame again when this pass ends
  bool full_resend_{false};  // this pass sends every row, whatever the shadow copy says
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

  // Connection tracking (see check_connection_()).
  uint8_t consecutive_failures_{0};
  bool connection_lost_{false};
  bool resync_pending_{false};  // send the settings and redraw everything at the next transfer
  bool config_dirty_{false};    // a settings command failed; send the settings again

  uint8_t brightness_{255};
  bool invert_{false};
  bool sleeping_{false};
  uint8_t fw_major_{0};
  uint8_t fw_minor_{0};
};

}  // namespace esphome::m5stack_unit_lcd
