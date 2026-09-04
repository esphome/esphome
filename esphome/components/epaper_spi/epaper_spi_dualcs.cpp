#include "epaper_spi_dualcs.h"

#include <algorithm>

#include "esphome/core/log.h"

namespace esphome::epaper_spi {

static constexpr const char *const TAG = "epaper_spi.dualcs";

void EPaperDualCS::setup() {
  EPaperBase::setup();
  // Both chip-selects are driven directly by this driver (the dual-CS protocol needs one
  // held HIGH while the other receives data, which the SPI bus abstraction cannot do on its
  // own). Start both deselected (HIGH).
  this->cs_pin_->setup();
  this->cs_pin_->digital_write(true);
  this->cs1_pin_->setup();
  this->cs1_pin_->digital_write(true);
}

void EPaperDualCS::dump_config() {
  EPaperBase::dump_config();
  LOG_PIN("  CS Pin: ", this->cs_pin_);
  LOG_PIN("  CS1 Pin: ", this->cs1_pin_);
}

// A normal update() always does a full refresh; only start_partial_update_() does a
// sub-rectangle. transfer_sub_ decides which path transfer_data() takes, so it has to be
// reset here, before the state machine starts.
void EPaperDualCS::update() {
  this->partial_update_ = false;
  this->transfer_sub_ = TRF_FULL;
  EPaperBase::update();
}

void EPaperDualCS::start_partial_update_(int x, int y, int w, int h) {
  if (this->state_ != EPaperState::IDLE) {
    ESP_LOGW(TAG, "start_partial_update_(): skipped, display busy (state %s)", this->epaper_state_to_string_());
    return;
  }
  this->partial_x_ = x;
  this->partial_y_ = y;
  this->partial_w_ = w;
  this->partial_h_ = h;
  this->partial_update_ = true;
  this->transfer_sub_ = TRF_PARTIAL_SETUP_M;
  this->compute_ptlw_params_();
  this->enable_loop();
  this->set_state_(EPaperState::RESET);
}

void EPaperDualCS::write_command_to_chip_(uint8_t cmd, const uint8_t *data, size_t len, uint8_t chip) {
  ESP_LOGD(TAG, "write_command_to_chip_: cmd=0x%02X len=%u chip=%u", cmd, (unsigned) len, (unsigned) chip);
  if (chip & CHIP_PRIMARY)
    this->cs_pin_->digital_write(false);
  if (chip & CHIP_SECONDARY)
    this->cs1_pin_->digital_write(false);

  if (this->toggle_dc_)
    this->dc_pin_->digital_write(false);
  this->enable();
  this->write_byte(cmd);
  if (len > 0 && data != nullptr) {
    if (this->toggle_dc_)
      this->dc_pin_->digital_write(true);
    this->write_array(data, len);
  }
  this->disable();

  if (chip & CHIP_PRIMARY)
    this->cs_pin_->digital_write(true);
  if (chip & CHIP_SECONDARY)
    this->cs1_pin_->digital_write(true);
}

void EPaperDualCS::log_busy_state_(const char *where) {
  uint32_t now = millis();
  if (now - this->wait_log_ms_ < 1000)
    return;
  this->wait_log_ms_ = now;
  ESP_LOGD(TAG, "%s: waiting -- busy_pin raw digital_read=%d, is_idle_()=%d", where,
           (int) this->busy_pin_->digital_read(), (int) this->is_idle_());
}

// Maps the requested logical (rotated) rectangle to physical panel columns/rows, then
// splits that physical window across the two chips (primary: left half, secondary: right
// half). HRST/HRED are in half-column units, VRST/VRED in half-row units -- that's the
// panel's own addressing granularity, not something this code chose.
void EPaperDualCS::compute_ptlw_params_() {
  this->ptlw_primary_.needed = false;
  this->ptlw_secondary_.needed = false;

  int x = this->partial_x_, y = this->partial_y_, w = this->partial_w_, h = this->partial_h_;

  // Clip the requested region to the logical (rotation-aware) canvas.
  if (x < 0) {
    w += x;
    x = 0;
  }
  if (y < 0) {
    h += y;
    y = 0;
  }
  if (x + w > this->get_width())
    w = this->get_width() - x;
  if (y + h > this->get_height())
    h = this->get_height() - y;
  if (w <= 0 || h <= 0)
    return;

  const int16_t panel_w = (int16_t) this->width_;   // physical panel width
  const int16_t panel_h = (int16_t) this->height_;  // physical panel height

  // Map logical coords to physical col/row by running both rectangle corners through the
  // same effective_transform_ (rotation + mirror_x/mirror_y) that draw_pixel_at() uses via
  // rotate_coordinates_(). This has to be the same transform as draw_pixel_at, or the window
  // pushed to the panel and the pixels actually drawn land in different places -- rotation_
  // alone isn't enough on boards that also need a static mirror.
  int x0 = x, y0 = y, x1 = x + w - 1, y1 = y + h - 1;
  if (this->effective_transform_ & SWAP_XY) {
    std::swap(x0, y0);
    std::swap(x1, y1);
  }
  if (this->effective_transform_ & MIRROR_X) {
    x0 = this->width_ - x0 - 1;
    x1 = this->width_ - x1 - 1;
  }
  if (this->effective_transform_ & MIRROR_Y) {
    y0 = this->height_ - y0 - 1;
    y1 = this->height_ - y1 - 1;
  }
  int16_t col_start = (int16_t) std::min(x0, x1);
  int16_t col_end = (int16_t) std::max(x0, x1);
  int16_t row_start = (int16_t) std::min(y0, y1);
  int16_t row_end = (int16_t) std::max(y0, y1);

  // Align to hardware constraints: columns start/end on multiples of 4, rows in pairs.
  col_start = (col_start / 4) * 4;
  col_end = (((col_end + 4) / 4) * 4) - 1;
  if (col_end >= panel_w)
    col_end = panel_w - 1;
  if (row_start % 2)
    row_start--;
  if (row_start < 0)
    row_start = 0;
  if ((row_end + 1) % 2)
    row_end++;
  if (row_end >= panel_h)
    row_end = panel_h - 1;

  const int16_t half_w = panel_w / 2;     // physical cols per chip
  const int16_t half_bytes = half_w / 2;  // bytes per row per chip

  // Primary chip handles physical cols 0..half_w-1.
  if (col_start < half_w) {
    int16_t lcs = col_start;
    int16_t lce = (col_end < half_w) ? col_end : (int16_t) (half_w - 1);
    uint16_t hrst = (uint16_t) lcs * 2;
    uint16_t hred = (uint16_t) (lce + 1) * 2 - 1;
    uint16_t vrst = (uint16_t) row_start / 2;
    uint16_t vred = (uint16_t) (row_end + 1) / 2 - 1;
    auto &p = this->ptlw_primary_;
    p.ptlw[0] = hrst >> 8;
    p.ptlw[1] = hrst & 0xFF;
    p.ptlw[2] = hred >> 8;
    p.ptlw[3] = hred & 0xFF;
    p.ptlw[4] = vrst >> 8;
    p.ptlw[5] = vrst & 0xFF;
    p.ptlw[6] = vred >> 8;
    p.ptlw[7] = vred & 0xFF;
    p.ptlw[8] = 0x01;
    p.bytes_per_row = (lce - lcs + 1) / 2;
    p.mem_col_off = lcs / 2;
    p.row_start = row_start;
    p.row_end = row_end;
    p.needed = true;
  }

  // Secondary chip handles physical cols half_w..panel_w-1.
  if (col_end >= half_w) {
    int16_t lcs = (col_start >= half_w) ? (int16_t) (col_start - half_w) : 0;
    int16_t lce = col_end - half_w;
    uint16_t hrst = (uint16_t) lcs * 2;
    uint16_t hred = (uint16_t) (lce + 1) * 2 - 1;
    uint16_t vrst = (uint16_t) row_start / 2;
    uint16_t vred = (uint16_t) (row_end + 1) / 2 - 1;
    auto &p = this->ptlw_secondary_;
    p.ptlw[0] = hrst >> 8;
    p.ptlw[1] = hrst & 0xFF;
    p.ptlw[2] = hred >> 8;
    p.ptlw[3] = hred & 0xFF;
    p.ptlw[4] = vrst >> 8;
    p.ptlw[5] = vrst & 0xFF;
    p.ptlw[6] = vred >> 8;
    p.ptlw[7] = vred & 0xFF;
    p.ptlw[8] = 0x01;
    p.bytes_per_row = (lce - lcs + 1) / 2;
    p.mem_col_off = half_bytes + lcs / 2;
    p.row_start = row_start;
    p.row_end = row_end;
    p.needed = true;
  }
}

// T133A01's original full-refresh algorithm, now shared. CS is held LOW for the ENTIRE DTM
// data stream of each chip's phase -- toggling CS between chunks resets the controller's
// data pointer, causing only the last chunk to be retained -- so it stays asserted across
// timeout boundaries by NOT deselecting on yield. There is deliberately no busy-wait
// between the CS and CS1 phases (unlike the partial-update path below): neither the
// T133A01 nor the Inkplate 13 Spectra have needed one.
bool HOT EPaperDualCS::transfer_full_() {
  const uint32_t start_time = millis();
  const uint16_t bytes_per_half_row = this->width_ / 4;
  const uint16_t total_rows = this->height_;
  const uint16_t bytes_per_row = this->width_ / 2;
  uint8_t line_data[400] = {};

  size_t half = this->current_data_index_;

  // CCSET: select color set before data transfer (CS + CS1). Needs resending on every
  // refresh, not just once at init.
  if (half == 0) {
    this->write_command_to_chip_(REG_CCSET, {0x01}, CHIP_BOTH);
    this->wait_for_idle_(true);
    delay(10);
  }

  // --- CS phase: left half of each row via CS ---
  if (half < total_rows) {
    if (half == 0) {
      this->cs_pin_->digital_write(false);  // select CS
      this->cs1_pin_->digital_write(true);  // deselect CS1
      this->dc_pin_->digital_write(false);
      this->enable();
      this->write_byte(REG_DTM);
      this->dc_pin_->digital_write(true);
    }

    while (half < total_rows) {
      size_t buf_offset = half * bytes_per_row;
      for (uint16_t col = 0; col < bytes_per_half_row; col++) {
        line_data[col] = this->buffer_[buf_offset + col];
      }
      this->write_array(line_data, bytes_per_half_row);
      half++;
      this->current_data_index_ = half;

      if (millis() - start_time > MAX_TRANSFER_TIME) {
        break;
      }
    }
    if (half < total_rows) {
      return false;
    }
    ESP_LOGD(TAG, "transfer_full_: CS phase done");
    this->disable();
    this->cs_pin_->digital_write(true);  // deselect CS
  }

  // --- CS1 phase: right half of each row via CS1 ---
  if (half >= total_rows && half < total_rows * 2) {
    size_t cs1_row = half - total_rows;

    if (cs1_row == 0) {
      this->cs_pin_->digital_write(true);    // deselect CS
      this->cs1_pin_->digital_write(false);  // select CS1
      this->enable();
      this->dc_pin_->digital_write(false);
      this->write_byte(REG_DTM);
      this->dc_pin_->digital_write(true);
    }

    while (half < total_rows * 2) {
      size_t row = half - total_rows;
      size_t buf_offset = row * bytes_per_row + bytes_per_half_row;
      for (uint16_t col = 0; col < bytes_per_half_row; col++) {
        line_data[col] = this->buffer_[buf_offset + col];
      }
      this->write_array(line_data, bytes_per_half_row);
      half++;
      this->current_data_index_ = half;

      if (millis() - start_time > MAX_TRANSFER_TIME) {
        break;
      }
    }
    if (half < total_rows * 2) {
      return false;
    }
    ESP_LOGD(TAG, "transfer_full_: CS1 phase done");
    this->disable();
    this->cs1_pin_->digital_write(true);  // deselect CS1
  }

  this->current_data_index_ = 0;
  return true;
}

// buffer_ is a SplitBuffer (not guaranteed contiguous), so each row is staged through a
// small stack buffer before write_array(). (Partial-update path only -- transfer_full_()
// stages through its own buffer.)
bool EPaperDualCS::transfer_data() {
  if (this->transfer_sub_ == TRF_FULL) {
    return this->transfer_full_();
  }

  const size_t bytes_per_row = (size_t) this->width_ / 2;
  const size_t half = bytes_per_row / 2;  // bytes per row per chip
  uint8_t row_buf[512];

  switch (this->transfer_sub_) {
    case TRF_FULL:
      return true;  // unreachable, handled above

    // Null PTLW: 4-col x 4-row window at the physical origin of this chip, sent to a chip
    // the requested rectangle doesn't touch so it still completes CMD66->PTLW->DTM.
    case TRF_PARTIAL_SETUP_M: {
      this->write_command_to_chip_(REG_CMD66, CMD66_V, sizeof(CMD66_V), CHIP_PRIMARY);
      if (!this->ptlw_primary_.needed) {
        this->write_command_to_chip_(REG_PTLW, NULL_PTLW, 9, CHIP_PRIMARY);
        this->cs_pin_->digital_write(false);
        this->enable();
        this->write_byte(REG_DTM);
        for (int r = 0; r < 4; r++) {
          size_t row_off = (size_t) r * bytes_per_row;
          for (size_t j = 0; j < 2; j++)
            row_buf[j] = this->buffer_[row_off + j];
          this->write_array(row_buf, 2);
        }
        this->disable();
        this->cs_pin_->digital_write(true);
        this->transfer_sub_ = TRF_PARTIAL_WAIT_M;
      } else {
        ESP_LOGD(TAG, "transfer: partial primary CMD66+PTLW+DTM start");
        this->write_command_to_chip_(REG_PTLW, this->ptlw_primary_.ptlw, 9, CHIP_PRIMARY);
        this->cs_pin_->digital_write(false);
        this->enable();
        this->write_byte(REG_DTM);
        this->transfer_row_ = (size_t) this->ptlw_primary_.row_start;
        this->transfer_sub_ = TRF_PARTIAL_DATA_M;
      }
      return false;
    }

    case TRF_PARTIAL_DATA_M: {
      size_t end = std::min(this->transfer_row_ + ROWS_PER_CHUNK, (size_t) (this->ptlw_primary_.row_end + 1));
      for (size_t i = this->transfer_row_; i < end; i++) {
        size_t row_off = i * bytes_per_row + (size_t) this->ptlw_primary_.mem_col_off;
        for (int j = 0; j < this->ptlw_primary_.bytes_per_row; j++)
          row_buf[j] = this->buffer_[row_off + j];
        this->write_array(row_buf, this->ptlw_primary_.bytes_per_row);
      }
      this->transfer_row_ = end;
      if (this->transfer_row_ > (size_t) this->ptlw_primary_.row_end) {
        this->disable();
        this->cs_pin_->digital_write(true);
        this->transfer_sub_ = TRF_PARTIAL_WAIT_M;
        ESP_LOGD(TAG, "transfer: partial primary DTM done");
      }
      return false;
    }

    case TRF_PARTIAL_WAIT_M:
      if (!this->is_idle_()) {
        this->log_busy_state_("transfer_data(): TRF_PARTIAL_WAIT_M");
        return false;
      }
      this->transfer_sub_ = TRF_PARTIAL_SETUP_S;
      return false;

    case TRF_PARTIAL_SETUP_S: {
      this->write_command_to_chip_(REG_CMD66, CMD66_V, sizeof(CMD66_V), CHIP_SECONDARY);
      if (!this->ptlw_secondary_.needed) {
        this->write_command_to_chip_(REG_PTLW, NULL_PTLW, 9, CHIP_SECONDARY);
        this->cs1_pin_->digital_write(false);
        this->enable();
        this->write_byte(REG_DTM);
        for (int r = 0; r < 4; r++) {
          size_t row_off = (size_t) r * bytes_per_row + half;
          for (size_t j = 0; j < 2; j++)
            row_buf[j] = this->buffer_[row_off + j];
          this->write_array(row_buf, 2);
        }
        this->disable();
        this->cs1_pin_->digital_write(true);
        this->transfer_sub_ = TRF_PARTIAL_WAIT_S;
      } else {
        ESP_LOGD(TAG, "transfer: partial secondary CMD66+PTLW+DTM start");
        this->write_command_to_chip_(REG_PTLW, this->ptlw_secondary_.ptlw, 9, CHIP_SECONDARY);
        this->cs1_pin_->digital_write(false);
        this->enable();
        this->write_byte(REG_DTM);
        this->transfer_row_ = (size_t) this->ptlw_secondary_.row_start;
        this->transfer_sub_ = TRF_PARTIAL_DATA_S;
      }
      return false;
    }

    case TRF_PARTIAL_DATA_S: {
      size_t end = std::min(this->transfer_row_ + ROWS_PER_CHUNK, (size_t) (this->ptlw_secondary_.row_end + 1));
      for (size_t i = this->transfer_row_; i < end; i++) {
        size_t row_off = i * bytes_per_row + (size_t) this->ptlw_secondary_.mem_col_off;
        for (int j = 0; j < this->ptlw_secondary_.bytes_per_row; j++)
          row_buf[j] = this->buffer_[row_off + j];
        this->write_array(row_buf, this->ptlw_secondary_.bytes_per_row);
      }
      this->transfer_row_ = end;
      if (this->transfer_row_ > (size_t) this->ptlw_secondary_.row_end) {
        this->disable();
        this->cs1_pin_->digital_write(true);
        this->transfer_sub_ = TRF_PARTIAL_WAIT_S;
        ESP_LOGD(TAG, "transfer: partial secondary DTM done");
      }
      return false;
    }

    case TRF_PARTIAL_WAIT_S:
      if (!this->is_idle_()) {
        this->log_busy_state_("transfer_data(): TRF_PARTIAL_WAIT_S");
        return false;
      }
      this->transfer_sub_ = TRF_DONE;
      return true;

    case TRF_DONE:
      return true;
  }
  return false;
}

}  // namespace esphome::epaper_spi
