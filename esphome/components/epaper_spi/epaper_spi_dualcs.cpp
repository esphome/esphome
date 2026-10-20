#include "epaper_spi_dualcs.h"

#include <algorithm>

#include "esphome/core/log.h"

namespace esphome::epaper_spi {

ESPHOME_LOG_TAG(TAG, "epaper_spi.dualcs");

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

void EPaperDualCS::write_command_to_chip_(uint8_t cmd, const uint8_t *data, size_t len, uint8_t chip) {
  ESP_LOGV(TAG, "write_command_to_chip_: cmd=0x%02X len=%u chip=%u", cmd, (unsigned) len, (unsigned) chip);
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

// Splits the dirty box (physical panel coordinates, already clipped and transformed by
// rotate_coordinates_(), x_high_/y_high_ exclusive) across the two chips (primary: left half,
// secondary: right half). HRST/HRED are in half-column units, VRST/VRED in half-row units --
// that's the panel's own addressing granularity, not something this code chose.
bool EPaperDualCS::compute_ptlw_params_() {
  this->ptlw_primary_.needed = false;
  this->ptlw_secondary_.needed = false;

  if (this->x_high_ <= this->x_low_ || this->y_high_ <= this->y_low_)
    return false;

  const int16_t panel_w = (int16_t) this->width_;  // physical panel width

  // Align to hardware constraints: columns start/end on multiples of 4, rows in pairs.
  int16_t col_start = this->x_low_ & ~3;
  int16_t col_end = (this->x_high_ - 1) | 3;
  int16_t row_start = this->y_low_ & ~1;
  int16_t row_end = (this->y_high_ - 1) | 1;

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
  return true;
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
  uint8_t line_data[MAX_HALF_ROW_BYTES] = {};

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
    ESP_LOGV(TAG, "transfer_full_: CS phase done");
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
    ESP_LOGV(TAG, "transfer_full_: CS1 phase done");
    this->disable();
    this->cs1_pin_->digital_write(true);  // deselect CS1
  }

  this->current_data_index_ = 0;
  return true;
}

// Full refresh unless the update is a partial one (partial_) with a non-empty dirty box. The
// path is chosen on entry, while transfer_sub_ is TRF_START, and the state returns to TRF_START
// once the transfer completes so the next update chooses again.
// buffer_ is a SplitBuffer (not guaranteed contiguous), so each partial row is staged through a
// small stack buffer before write_array() (transfer_full_() stages through its own buffer).
bool EPaperDualCS::transfer_data() {
  const size_t bytes_per_row = (size_t) this->width_ / 2;
  const size_t half = bytes_per_row / 2;  // bytes per row per chip
  uint8_t row_buf[MAX_HALF_ROW_BYTES];

  switch (this->transfer_sub_) {
    case TRF_START:
      this->transfer_sub_ = this->partial_ && this->compute_ptlw_params_() ? TRF_PARTIAL_SETUP_M : TRF_FULL;
      return this->transfer_data();

    case TRF_FULL:
      if (!this->transfer_full_())
        return false;
      this->transfer_sub_ = TRF_START;
      return true;

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
        ESP_LOGV(TAG, "transfer: partial primary CMD66+PTLW+DTM start");
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
        ESP_LOGV(TAG, "transfer: partial primary DTM done");
      }
      return false;
    }

    case TRF_PARTIAL_WAIT_M:
      // Let EPaperBase do the waiting, so its busy-pin timeout also covers this wait.
      if (!this->is_idle_()) {
        this->wait_for_idle_(true);
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
        ESP_LOGV(TAG, "transfer: partial secondary CMD66+PTLW+DTM start");
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
        ESP_LOGV(TAG, "transfer: partial secondary DTM done");
      }
      return false;
    }

    case TRF_PARTIAL_WAIT_S:
      // Let EPaperBase do the waiting, so its busy-pin timeout also covers this wait.
      if (!this->is_idle_()) {
        this->wait_for_idle_(true);
        return false;
      }
      this->transfer_sub_ = TRF_START;
      return true;
  }
  return false;
}

void EPaperDualCS::abort_transfer() {
  // A DTM data stream is open, with one of the chip selects held low, while the full path has
  // sent rows (current_data_index_ != 0) or the partial path is in one of its DATA states.
  bool stream_open = this->transfer_sub_ == TRF_FULL
                         ? this->current_data_index_ != 0
                         : (this->transfer_sub_ == TRF_PARTIAL_DATA_M || this->transfer_sub_ == TRF_PARTIAL_DATA_S);
  if (stream_open) {
    this->disable();
    this->cs_pin_->digital_write(true);
    this->cs1_pin_->digital_write(true);
  }
  // The next transfer chooses its path again
  this->transfer_sub_ = TRF_START;
}

}  // namespace esphome::epaper_spi
