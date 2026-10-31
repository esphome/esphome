#include "m5stack_unit_lcd.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

#include "esphome/components/display/display_color_utils.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::m5stack_unit_lcd {

ESPHOME_LOG_TAG(TAG, "m5stack_unit_lcd");

// Unit LCD I2C command set. References: https://docs.m5stack.com/en/unit/lcd, the Panel_M5UnitLCD
// driver in M5GFX, and the unit's firmware at https://github.com/m5stack/M5Unit-LCD-Internal-FW.
static constexpr uint8_t CMD_READ_ID = 0x04;        // -> 0x77 0x89 <major> <minor>
static constexpr uint8_t CMD_READ_BUFCOUNT = 0x09;  // -> free command-buffer space (1-255)
static constexpr uint8_t CMD_INVOFF = 0x20;
static constexpr uint8_t CMD_INVON = 0x21;
static constexpr uint8_t CMD_BRIGHTNESS = 0x22;    // [1]=0-255
static constexpr uint8_t CMD_CASET = 0x2A;         // [1]=xs [2]=xe
static constexpr uint8_t CMD_RASET = 0x2B;         // [1]=ys [2]=ye
static constexpr uint8_t CMD_ROTATE = 0x36;        // [1]=0-7
static constexpr uint8_t CMD_SET_POWER = 0x38;     // [1]=0 low / 1 normal / 2 high
static constexpr uint8_t CMD_SET_SLEEP = 0x39;     // [1]=0 wake / 1 sleep
static constexpr uint8_t CMD_WRITE_RAW_16 = 0x42;  // RGB565 pixels, big-endian, until STOP
static constexpr uint8_t CMD_FILLRECT_16 = 0x6A;   // [1]=xs [2]=ys [3]=xe [4]=ye [5-6]=RGB565

static constexpr uint8_t ID_BYTE_0 = 0x77;
static constexpr uint8_t ID_BYTE_1 = 0x89;

// Command-buffer flow control. The unit queues each command and each pixel in an 8192-entry ring
// that is not checked for overflow, and READ_BUFCOUNT reports about (512 - queued) / 2, or 255
// when the queue is empty. Charging one unit per four pixels, as M5GFX does, keeps the backlog
// near 1000 entries.
static constexpr int BUFCOUNT_LOW_WATER = 32;
static constexpr int BUFCOUNT_REFILL = 128;
static constexpr uint32_t BUFCOUNT_TIMEOUT_MS = 500;

// Failed transactions in a row before the unit is treated as gone.
static constexpr uint8_t MAX_CONSECUTIVE_FAILURES = 3;

// Time budget for pushing pixels per loop() call. A 263-byte transaction takes about 7 ms at
// 400 kHz, so this normally means one or two chunks per loop iteration.
static constexpr uint32_t MAX_TRANSFER_TIME_MS = 10;

void M5StackUnitLCD::setup() {
  if (!this->read_id_()) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  if (!this->buffer_.init(FRAME_BYTES) || !this->prev_.init(FRAME_BYTES)) {
    this->mark_failed(LOG_STR("Failed to allocate frame buffers"));
    return;
  }
  if (!this->send_config_()) {
    this->mark_failed(LOG_STR("Failed to configure panel"));
    return;
  }
  // Blank the panel so it matches the (all-black) shadow buffer.
  const uint8_t blank[] = {CMD_FILLRECT_16, 0, 0, WIDTH - 1, HEIGHT - 1, 0x00, 0x00};
  this->consume_credit_(1);
  if (!this->send_(blank, sizeof(blank))) {
    this->mark_failed(LOG_STR("Failed to clear panel"));
    return;
  }
  this->disable_loop();  // loop() only runs while a transfer is in progress
}

void M5StackUnitLCD::dump_config() {
  LOG_DISPLAY("", "M5Stack Unit LCD", this);
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG,
                "  Firmware: %u.%u\n"
                "  Brightness: %u%%\n"
                "  Invert colors: %s",
                this->fw_major_, this->fw_minor_, static_cast<unsigned>(this->brightness_) * 100 / 255,
                YESNO(this->invert_));
  LOG_UPDATE_INTERVAL(this);
}

void M5StackUnitLCD::update() {
  if (this->is_failed() || !this->buffer_.is_valid())
    return;
  this->do_update_();
  if (this->transfer_active_) {
    // Still pushing the previous frame; rescan once that pass finishes.
    this->rescan_ = true;
    return;
  }
  if (this->check_connection_())
    this->start_transfer_();
}

bool M5StackUnitLCD::check_connection_() {
  if (this->connection_lost_) {
    if (!this->read_id_())
      return false;  // still gone; try again at the next update
    this->connection_lost_ = false;
    this->note_bus_ok_();
  } else {
    // A cheap probe that also refreshes the flow-control credit. It notices a unit that was
    // unplugged while the screen had nothing to send.
    uint8_t free_slots;
    if (this->write_read(&CMD_READ_BUFCOUNT, 1, &free_slots, 1) != i2c::ERROR_OK) {
      this->lose_connection_();
      return false;
    }
    this->buf_free_ = free_slots;
  }
  if (this->resync_pending_ || this->config_dirty_) {
    if (!this->send_config_())
      return false;
    this->config_dirty_ = false;
  }
  return true;
}

void M5StackUnitLCD::start_transfer_() {
  this->transfer_active_ = true;
  this->rescan_ = false;
  this->full_resend_ = this->resync_pending_;
  this->resync_pending_ = false;
  this->next_row_ = 0;
  this->region_active_ = false;
  this->waiting_for_credit_ = false;
  this->transfer_start_ = App.get_loop_component_start_time();
  this->transfer_pixels_ = 0;
  this->transfer_tx_ = 0;
  this->enable_loop();
}

void M5StackUnitLCD::loop() {
  if (!this->transfer_active_) {
    this->disable_loop();
    return;
  }
  // Each I2C chunk takes several milliseconds, so the slice is measured with sub-tick resolution
  // from the moment it starts rather than from the cached loop timestamp.
  const uint32_t start = millis();
  do {
    if (!this->region_active_ && !this->find_next_region_()) {
      this->finish_transfer_();
      return;
    }
    if (!this->send_next_chunk_())
      return;  // waiting for the unit, or a bus error: retried on the next loop iteration
  } while (millis() - start < MAX_TRANSFER_TIME_MS);
}

int M5StackUnitLCD::get_width() {
  switch (this->rotation_) {
    case display::DISPLAY_ROTATION_90_DEGREES:
    case display::DISPLAY_ROTATION_270_DEGREES:
      return HEIGHT;
    default:
      return WIDTH;
  }
}

int M5StackUnitLCD::get_height() {
  switch (this->rotation_) {
    case display::DISPLAY_ROTATION_90_DEGREES:
    case display::DISPLAY_ROTATION_270_DEGREES:
      return WIDTH;
    default:
      return HEIGHT;
  }
}

void HOT M5StackUnitLCD::draw_pixel_at(int x, int y, Color color) {
  if (!this->get_clipping().inside(x, y))
    return;  // NOLINT
  switch (this->rotation_) {
    case display::DISPLAY_ROTATION_0_DEGREES:
      break;
    case display::DISPLAY_ROTATION_90_DEGREES:
      std::swap(x, y);
      x = WIDTH - x - 1;
      break;
    case display::DISPLAY_ROTATION_180_DEGREES:
      x = WIDTH - x - 1;
      y = HEIGHT - y - 1;
      break;
    case display::DISPLAY_ROTATION_270_DEGREES:
      std::swap(x, y);
      y = HEIGHT - y - 1;
      break;
  }
  this->set_native_pixel_(x, y, display::ColorUtil::color_to_565(color));
  this->feed_wdt_per_pixel_();
}

void M5StackUnitLCD::set_native_pixel_(int x, int y, uint16_t rgb565) {
  if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT)
    return;
  const size_t i = (static_cast<size_t>(y) * WIDTH + x) * BYTES_PER_PIXEL;
  const uint8_t hi = rgb565 >> 8;
  const uint8_t lo = rgb565 & 0xFF;
  size_t len;
  uint8_t *p = this->buffer_.get_span(i, len);
  if (p == nullptr)
    return;  // frame buffer not allocated
  // When the allocation was split, a pixel's two bytes can sit in different chunks.
  uint8_t &p_lo = len >= BYTES_PER_PIXEL ? p[1] : this->buffer_[i + 1];
  if (p[0] == hi && p_lo == lo)
    return;
  p[0] = hi;
  p_lo = lo;
  this->mark_row_touched_(y);
}

void M5StackUnitLCD::fill(Color color) {
  if (this->is_clipping()) {
    display::Display::fill(color);  // pixel by pixel, honouring the clip rectangle
    return;
  }
  if (!this->buffer_.is_valid())
    return;
  const uint16_t c = display::ColorUtil::color_to_565(color);
  const uint8_t hi = c >> 8;
  const uint8_t lo = c & 0xFF;
  if (hi == lo) {
    this->buffer_.fill(hi);
  } else {
    size_t index = 0;
    while (index < FRAME_BYTES) {
      size_t len;
      uint8_t *p = this->buffer_.get_span(index, len);
      if (p == nullptr)
        break;
      for (size_t k = 0; k < len; k++)
        p[k] = ((index + k) % BYTES_PER_PIXEL == 0) ? hi : lo;
      index += len;
    }
  }
  this->touched_rows_.fill(0xFF);
}

void M5StackUnitLCD::set_brightness(float brightness) {
  this->brightness_ = static_cast<uint8_t>(lroundf(clamp(brightness, 0.0f, 1.0f) * 255.0f));
  const uint8_t cmd[] = {CMD_BRIGHTNESS, this->brightness_};
  this->send_command_(cmd, sizeof(cmd));
}

void M5StackUnitLCD::set_invert_colors(bool invert) {
  this->invert_ = invert;
  const uint8_t cmd = invert ? CMD_INVON : CMD_INVOFF;
  this->send_command_(&cmd, 1);
}

void M5StackUnitLCD::set_sleep(bool enable) {
  this->sleeping_ = enable;
  const uint8_t cmd[] = {CMD_SET_SLEEP, static_cast<uint8_t>(enable)};
  this->send_command_(cmd, sizeof(cmd));
}

void M5StackUnitLCD::read_frame_(size_t index, uint8_t *dst, size_t len) {
  while (len > 0) {
    size_t span;
    const uint8_t *src = this->buffer_.get_span(index, span);
    if (src == nullptr)
      return;
    span = std::min(span, len);
    std::memcpy(dst, src, span);
    index += span;
    dst += span;
    len -= span;
  }
}

void M5StackUnitLCD::mirror_frame_(size_t index, size_t len) {
  while (len > 0) {
    size_t span;
    const uint8_t *src = this->buffer_.get_span(index, span);
    if (src == nullptr)
      return;
    span = std::min(span, len);
    this->prev_.write(index, src, span);
    index += span;
    len -= span;
  }
}

bool M5StackUnitLCD::read_id_() {
  uint8_t buf[4] = {0, 0, 0, 0};
  if (this->write_read(&CMD_READ_ID, 1, buf, sizeof(buf)) != i2c::ERROR_OK)
    return false;
  if (buf[0] != ID_BYTE_0 || buf[1] != ID_BYTE_1) {
    ESP_LOGW(TAG, "Unexpected ID: %02X %02X %02X %02X", buf[0], buf[1], buf[2], buf[3]);
    return false;
  }
  this->fw_major_ = buf[2];
  this->fw_minor_ = buf[3];
  return true;
}

void M5StackUnitLCD::note_bus_error_() {
  this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
  if (++this->consecutive_failures_ >= MAX_CONSECUTIVE_FAILURES)
    this->lose_connection_();
}

void M5StackUnitLCD::note_bus_ok_() {
  this->consecutive_failures_ = 0;
  this->status_clear_warning();
}

void M5StackUnitLCD::lose_connection_() {
  // Stop retrying until the next update; a unit that comes back has rebooted, so send the
  // settings again and redraw everything.
  this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
  this->connection_lost_ = true;
  this->resync_pending_ = true;
  this->transfer_active_ = false;
  this->region_active_ = false;
  this->buf_free_ = 0;
  this->disable_loop();
}

bool M5StackUnitLCD::send_(const uint8_t *data, size_t len) {
  if (this->write(data, len) != i2c::ERROR_OK) {
    this->note_bus_error_();
    return false;
  }
  this->note_bus_ok_();
  return true;
}

void M5StackUnitLCD::send_command_(const uint8_t *data, size_t len) {
  if (!this->is_ready() || this->connection_lost_)
    return;
  this->consume_credit_(1);
  if (!this->send_(data, len))
    this->config_dirty_ = true;
}

bool M5StackUnitLCD::send_config_() {
  // Fixed-length commands may be concatenated in one transaction. Sleep goes last: waking up
  // restores the backlight brightness sent before it, and going to sleep turns the backlight off.
  const uint8_t cfg[] = {
      CMD_SET_POWER,
      1,  // normal power
      CMD_ROTATE,
      0,  // native portrait; rotation is done in software
      CMD_BRIGHTNESS,
      this->brightness_,
      this->invert_ ? CMD_INVON : CMD_INVOFF,
      CMD_SET_SLEEP,
      static_cast<uint8_t>(this->sleeping_),
  };
  this->consume_credit_(5);
  return this->send_(cfg, sizeof(cfg));
}

M5StackUnitLCD::CreditResult M5StackUnitLCD::reserve_(int cost) {
  if (this->buf_free_ >= cost + BUFCOUNT_LOW_WATER) {
    this->buf_free_ -= cost;
    this->waiting_for_credit_ = false;
    return CreditResult::CREDIT_RESULT_GRANTED;
  }
  uint8_t free_slots = 0;
  if (this->write_read(&CMD_READ_BUFCOUNT, 1, &free_slots, 1) != i2c::ERROR_OK) {
    this->note_bus_error_();
    return CreditResult::CREDIT_RESULT_BUS_ERROR;
  }
  this->note_bus_ok_();
  if (free_slots >= BUFCOUNT_REFILL) {
    this->buf_free_ = free_slots - cost;
    this->waiting_for_credit_ = false;
    return CreditResult::CREDIT_RESULT_GRANTED;
  }
  const uint32_t now = App.get_loop_component_start_time();
  if (!this->waiting_for_credit_) {
    this->waiting_for_credit_ = true;
    this->credit_wait_start_ = now;
    return CreditResult::CREDIT_RESULT_WAIT;
  }
  if (now - this->credit_wait_start_ < BUFCOUNT_TIMEOUT_MS)
    return CreditResult::CREDIT_RESULT_WAIT;
  // The unit answers but never reports enough free space; carry on rather than stall forever.
  ESP_LOGW(TAG, "Command buffer did not drain (free=%u); continuing", free_slots);
  this->waiting_for_credit_ = false;
  this->buf_free_ = 0;
  return CreditResult::CREDIT_RESULT_GRANTED;
}

bool M5StackUnitLCD::row_dirty_(int y, int &x0, int &x1) {
  if (this->full_resend_) {
    x0 = 0;
    x1 = WIDTH - 1;
    return true;
  }
  if (!this->row_touched_(y))
    return false;
  // Compare a row of both buffers a contiguous piece at a time; a piece ends at a chunk boundary
  // of either buffer when the allocation was split.
  const size_t row = static_cast<size_t>(y) * ROW_BYTES;
  size_t first = ROW_BYTES;
  size_t last = 0;
  for (size_t offset = 0; offset < ROW_BYTES;) {
    size_t a_len;
    size_t b_len;
    const uint8_t *a = this->buffer_.get_span(row + offset, a_len);
    const uint8_t *b = this->prev_.get_span(row + offset, b_len);
    if (a == nullptr || b == nullptr)
      break;
    const size_t n = std::min({a_len, b_len, ROW_BYTES - offset});
    if (std::memcmp(a, b, n) != 0) {
      if (first == ROW_BYTES) {
        size_t i = 0;
        while (a[i] == b[i])
          i++;
        first = offset + i;
      }
      size_t j = n - 1;
      while (a[j] == b[j])
        j--;
      last = offset + j;
    }
    offset += n;
  }
  if (first == ROW_BYTES) {
    // Touched but ended up identical (e.g. cleared and redrawn with the same content).
    this->clear_row_touched_(y);
    return false;
  }
  x0 = static_cast<int>(first / BYTES_PER_PIXEL);
  x1 = static_cast<int>(last / BYTES_PER_PIXEL);
  return true;
}

bool M5StackUnitLCD::find_next_region_() {
  while (true) {
    while (this->next_row_ < HEIGHT) {
      int x0, x1;
      if (this->row_dirty_(this->next_row_, x0, x1)) {
        // Extend over consecutive dirty rows, tracking the union of their spans.
        int y1 = this->next_row_;
        while (y1 + 1 < HEIGHT) {
          int a, b;
          if (!this->row_dirty_(y1 + 1, a, b))
            break;
          x0 = std::min(x0, a);
          x1 = std::max(x1, b);
          y1++;
        }
        this->region_xs_ = x0;
        this->region_xe_ = x1;
        this->region_y_ = this->next_row_;
        this->region_y_end_ = y1;
        this->region_x_ = 0;
        this->region_started_ = false;
        this->region_active_ = true;
        this->next_row_ = y1 + 1;
        return true;
      }
      this->next_row_++;
    }
    this->full_resend_ = false;
    if (!this->rescan_)
      return false;
    this->rescan_ = false;
    this->next_row_ = 0;
  }
}

bool M5StackUnitLCD::send_next_chunk_() {
  const size_t w = static_cast<size_t>(this->region_xe_ - this->region_xs_ + 1);
  const size_t remaining =
      static_cast<size_t>(this->region_y_end_ - this->region_y_) * w + (w - static_cast<size_t>(this->region_x_));
  const size_t px = std::min(MAX_PIXELS_PER_TX, remaining);
  // Window commands cost 3 units; pixel data about one unit per four pixels.
  const int cost = (this->region_started_ ? 1 : 3) + static_cast<int>(px / 4) + 1;
  if (this->reserve_(cost) != CreditResult::CREDIT_RESULT_GRANTED)
    return false;  // nothing sent, state unchanged

  size_t len = 0;
  if (!this->region_started_) {
    this->tx_[len++] = CMD_CASET;
    this->tx_[len++] = static_cast<uint8_t>(this->region_xs_);
    this->tx_[len++] = static_cast<uint8_t>(this->region_xe_);
    this->tx_[len++] = CMD_RASET;
    this->tx_[len++] = static_cast<uint8_t>(this->region_y_);
    this->tx_[len++] = static_cast<uint8_t>(this->region_y_end_);
  }
  this->tx_[len++] = CMD_WRITE_RAW_16;

  // Visit the next px pixels of the region one row segment at a time: f(byte offset, byte count).
  const auto for_each_segment = [this, w, px](auto &&f) {
    size_t x = this->region_x_;
    size_t y = this->region_y_;
    for (size_t left = px; left > 0;) {
      const size_t n = std::min(w - x, left);
      f((y * WIDTH + this->region_xs_ + x) * BYTES_PER_PIXEL, n * BYTES_PER_PIXEL);
      left -= n;
      x += n;
      if (x == w) {
        x = 0;
        y++;
      }
    }
  };
  for_each_segment([this, &len](size_t offset, size_t bytes) {
    this->read_frame_(offset, this->tx_ + len, bytes);
    len += bytes;
  });

  if (!this->send_(this->tx_, len)) {
    // The panel's write pointer is now unknown. Drop the region; whatever was not mirrored into
    // prev_ still differs from the frame buffer and will be resent with a fresh window.
    this->region_active_ = false;
    this->next_row_ = std::min(this->next_row_, this->region_y_);
    return false;
  }

  // Mirror what was just sent so it is not sent again. The rows stay flagged as touched: an
  // update() between two chunks of the same row can change pixels that were already sent, and
  // the rescan pass that follows must compare those rows again rather than trust the flag.
  for_each_segment([this](size_t offset, size_t bytes) { this->mirror_frame_(offset, bytes); });
  const size_t advanced = static_cast<size_t>(this->region_x_) + px;
  this->region_y_ += static_cast<int>(advanced / w);
  this->region_x_ = static_cast<int>(advanced % w);
  this->region_started_ = true;
  this->transfer_pixels_ += px;
  this->transfer_tx_++;
  if (this->region_y_ > this->region_y_end_)
    this->region_active_ = false;
  return true;
}

void M5StackUnitLCD::finish_transfer_() {
  this->transfer_active_ = false;
  this->disable_loop();
  if (this->transfer_tx_ > 0) {
    ESP_LOGV(TAG, "Sent %zu px in %zu transfers, %u ms", this->transfer_pixels_, this->transfer_tx_,
             static_cast<unsigned>(App.get_loop_component_start_time() - this->transfer_start_));
  }
}

}  // namespace esphome::m5stack_unit_lcd
