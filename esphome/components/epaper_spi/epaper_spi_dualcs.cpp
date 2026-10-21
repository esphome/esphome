#include "epaper_spi_dualcs.h"
#include "colorconv.h"

#include <algorithm>

#include "esphome/core/log.h"

namespace esphome::epaper_spi {

ESPHOME_LOG_TAG(TAG, "epaper_spi.dualcs");

uint8_t EPaperDualCS::color_to_native(Color color) { return color_to_spectra6(color); }

void EPaperDualCS::power_on() {
  ESP_LOGV(TAG, "Power on");
  this->write_command_to_chip_(REG_PON, CHIP_BOTH);
}

// EPaperBase's automatic busy-wait before DEEP_SLEEP covers the wait for POF to complete.
void EPaperDualCS::power_off() {
  ESP_LOGV(TAG, "Power off");
  this->write_command_to_chip_(REG_POF, {0x00}, CHIP_BOTH);
}

void EPaperDualCS::refresh_screen(bool partial) {
  ESP_LOGV(TAG, "Refresh screen");
  this->write_command_to_chip_(REG_DRF, {0x01}, CHIP_BOTH);
}

void EPaperDualCS::deep_sleep() {
  ESP_LOGV(TAG, "Deep sleep");
  this->write_command_to_chip_(REG_DSLP, {0xA5}, CHIP_BOTH);
}

// Each entry is the command, a count byte, the chip mask and the data (the count covers the mask
// and the data); a delay of N ms is N then DELAY_FLAG. The Python model builds this layout with the
// standard flatten_sequence(), the chip mask riding as the first data byte.
bool EPaperDualCS::initialise(bool partial) {
  const uint8_t *sequence = this->init_sequence_;
  const size_t length = this->init_sequence_length_;
  while (this->init_index_ != length) {
    if (length - this->init_index_ < 2) {
      this->init_index_ = 0;
      this->mark_failed(LOG_STR("Malformed init sequence"));
      return false;
    }
    const uint8_t cmd = sequence[this->init_index_++];
    const uint8_t count = sequence[this->init_index_++];
    if (count == DELAY_FLAG) {
      ESP_LOGV(TAG, "Delay %u ms", cmd);
      this->delay_until_ = millis() + cmd;
      return false;
    }
    const uint8_t num_args = count & 0x7F;
    if (num_args == 0 || length - this->init_index_ < num_args) {
      this->init_index_ = 0;
      this->mark_failed(LOG_STR("Malformed init sequence"));
      return false;
    }
    const uint8_t *args = sequence + this->init_index_;
    this->write_command_to_chip_(cmd, args + 1, num_args - 1, args[0]);
    this->init_index_ += num_args;
  }
  this->init_index_ = 0;
  return true;
}

void EPaperDualCS::setup() {
  EPaperBase::setup();
  // The driver drives both chip selects itself: one must stay high while the other receives data.
  for (auto *cs : this->cs_pins_) {
    cs->setup();
    cs->digital_write(true);
  }
}

void EPaperDualCS::dump_config() {
  EPaperBase::dump_config();
  LOG_PIN("  CS Pin: ", this->cs_pins_[0]);
  LOG_PIN("  CS1 Pin: ", this->cs_pins_[1]);
}

void EPaperDualCS::write_command_to_chip_(uint8_t cmd, const uint8_t *data, size_t len, uint8_t chip) {
  ESP_LOGV(TAG, "write_command_to_chip_: cmd=0x%02X len=%u chip=%u", cmd, (unsigned) len, (unsigned) chip);
  for (uint8_t i = 0; i != CHIP_COUNT; i++) {
    if (chip & (1 << i))
      this->cs_pins_[i]->digital_write(false);
  }

  this->dc_pin_->digital_write(false);
  this->enable();
  this->write_byte(cmd);
  if (len > 0 && data != nullptr) {
    this->dc_pin_->digital_write(true);
    this->write_array(data, len);
  }
  this->disable();

  for (uint8_t i = 0; i != CHIP_COUNT; i++) {
    if (chip & (1 << i))
      this->cs_pins_[i]->digital_write(true);
  }
}

void EPaperDualCS::start_data_(uint8_t chip) {
  this->cs_pins_[chip]->digital_write(false);
  this->dc_pin_->digital_write(false);
  this->enable();
  this->write_byte(REG_DTM);
  this->dc_pin_->digital_write(true);
}

void EPaperDualCS::end_data_(uint8_t chip) {
  this->disable();
  this->cs_pins_[chip]->digital_write(true);
}

// Splits the dirty box (panel coordinates, x_high_/y_high_ exclusive) between the two chips.
// HRST/HRED count half columns and VRST/VRED half rows, as the panel addresses them.
bool EPaperDualCS::compute_ptlw_params_() {
  for (auto &p : this->ptlw_)
    p.needed = false;

  if (this->x_high_ <= this->x_low_ || this->y_high_ <= this->y_low_)
    return false;

  // Align to hardware constraints: columns start/end on multiples of 4, rows in pairs.
  const int16_t col_start = this->x_low_ & ~3;
  const int16_t col_end = (this->x_high_ - 1) | 3;
  const int16_t row_start = this->y_low_ & ~1;
  const int16_t row_end = (this->y_high_ - 1) | 1;

  const int16_t half_w = (int16_t) this->width_ / 2;  // physical cols per chip
  const uint16_t vrst = (uint16_t) row_start / 2;
  const uint16_t vred = (uint16_t) (row_end + 1) / 2 - 1;

  for (uint8_t chip = 0; chip != CHIP_COUNT; chip++) {
    const int16_t first = chip * half_w;
    const int16_t last = first + half_w - 1;
    if (col_end < first || col_start > last)
      continue;
    // Window columns relative to this chip
    const int16_t lcs = std::max(col_start, first) - first;
    const int16_t lce = std::min(col_end, last) - first;
    const uint16_t hrst = (uint16_t) lcs * 2;
    const uint16_t hred = (uint16_t) (lce + 1) * 2 - 1;
    auto &p = this->ptlw_[chip];
    const uint16_t words[4] = {hrst, hred, vrst, vred};
    for (size_t i = 0; i != 4; i++) {
      p.ptlw[i * 2] = words[i] >> 8;
      p.ptlw[i * 2 + 1] = words[i] & 0xFF;
    }
    p.ptlw[8] = 0x01;
    p.bytes_per_row = (lce - lcs + 1) / 2;
    p.mem_col_off = first / 2 + lcs / 2;
    p.row_start = row_start;
    p.row_end = row_end;
    p.needed = true;
  }
  return true;
}

// Each chip in turn receives its half of every row. Its CS stays low for the whole stream, even
// between calls: raising CS restarts the data pointer, so only the last chunk would be kept.
bool HOT EPaperDualCS::transfer_full_() {
  const uint32_t start_time = millis();
  const uint16_t half_row_bytes = this->width_ / 4;
  const size_t rows = this->height_;
  const size_t row_bytes = this->width_ / 2;
  uint8_t line_data[MAX_HALF_ROW_BYTES] = {};

  // Counts rows over both chips: chip 0 sends rows 0..rows-1, then chip 1 the same rows again.
  size_t index = this->current_data_index_;

  // The color set must be selected again before every full transfer, not only at init.
  if (index == 0) {
    this->write_command_to_chip_(REG_CCSET, {0x01}, CHIP_BOTH);
    this->wait_for_idle_(true);
    delay(10);
  }

  while (index < rows * CHIP_COUNT) {
    const uint8_t chip = index / rows;
    const size_t row = index % rows;
    if (row == 0)
      this->start_data_(chip);

    const size_t buf_offset = row * row_bytes + chip * half_row_bytes;
    for (uint16_t col = 0; col < half_row_bytes; col++)
      line_data[col] = this->buffer_[buf_offset + col];
    this->write_array(line_data, half_row_bytes);
    this->current_data_index_ = ++index;

    // A finished chip always closes its stream, even when the time is also up.
    if (index % rows == 0) {
      ESP_LOGV(TAG, "transfer_full_: chip %u done", (unsigned) chip);
      this->end_data_(chip);
    } else if (millis() - start_time > MAX_TRANSFER_TIME) {
      return false;
    }
  }

  this->current_data_index_ = 0;
  return true;
}

// Full transfer unless the update is a partial one with a non-empty dirty box. buffer_ is a
// SplitBuffer and may not be contiguous, so rows are copied to a stack buffer before sending.
bool EPaperDualCS::transfer_data() {
  const size_t bytes_per_row = (size_t) this->width_ / 2;
  const uint8_t chip = this->transfer_chip_;
  const auto &params = this->ptlw_[chip];
  uint8_t row_buf[MAX_HALF_ROW_BYTES];

  switch (this->transfer_sub_) {
    case TRF_START:
      this->transfer_chip_ = 0;
      this->transfer_sub_ = this->partial_ && this->compute_ptlw_params_() ? TRF_PARTIAL_SETUP : TRF_FULL;
      return this->transfer_data();

    case TRF_FULL:
      if (!this->transfer_full_())
        return false;
      this->transfer_sub_ = TRF_START;
      return true;

    case TRF_PARTIAL_SETUP: {
      const uint8_t mask = 1 << chip;
      this->write_command_to_chip_(REG_CMD66, CMD66_V, sizeof(CMD66_V), mask);
      if (!params.needed) {
        // Send 4 rows of the NULL_PTLW window
        this->write_command_to_chip_(REG_PTLW, NULL_PTLW, sizeof(NULL_PTLW), mask);
        this->start_data_(chip);
        for (size_t r = 0; r != 4; r++) {
          const size_t row_off = r * bytes_per_row + chip * (bytes_per_row / 2);
          for (size_t j = 0; j != 2; j++)
            row_buf[j] = this->buffer_[row_off + j];
          this->write_array(row_buf, 2);
        }
        this->end_data_(chip);
        this->transfer_sub_ = TRF_PARTIAL_WAIT;
      } else {
        ESP_LOGV(TAG, "transfer: partial chip %u CMD66+PTLW+DTM start", (unsigned) chip);
        this->write_command_to_chip_(REG_PTLW, params.ptlw, sizeof(params.ptlw), mask);
        this->start_data_(chip);
        this->transfer_row_ = (size_t) params.row_start;
        this->transfer_sub_ = TRF_PARTIAL_DATA;
      }
      return false;
    }

    case TRF_PARTIAL_DATA: {
      const size_t end = std::min(this->transfer_row_ + ROWS_PER_CHUNK, (size_t) (params.row_end + 1));
      for (size_t i = this->transfer_row_; i < end; i++) {
        const size_t row_off = i * bytes_per_row + (size_t) params.mem_col_off;
        for (int j = 0; j < params.bytes_per_row; j++)
          row_buf[j] = this->buffer_[row_off + j];
        this->write_array(row_buf, params.bytes_per_row);
      }
      this->transfer_row_ = end;
      if (this->transfer_row_ > (size_t) params.row_end) {
        this->end_data_(chip);
        this->transfer_sub_ = TRF_PARTIAL_WAIT;
        ESP_LOGV(TAG, "transfer: partial chip %u DTM done", (unsigned) chip);
      }
      return false;
    }

    case TRF_PARTIAL_WAIT:
      // Let EPaperBase do the waiting, so its busy-pin timeout also covers this wait.
      if (!this->is_idle_()) {
        this->wait_for_idle_(true);
        return false;
      }
      if (chip + 1 < CHIP_COUNT) {
        this->transfer_chip_ = chip + 1;
        this->transfer_sub_ = TRF_PARTIAL_SETUP;
        return false;
      }
      this->transfer_sub_ = TRF_START;
      return true;
  }
  return false;
}

void EPaperDualCS::abort_transfer() {
  // A DTM data stream is open, with one of the chip selects held low, while the full path has
  // sent rows (current_data_index_ != 0) or the partial path is in its DATA state.
  bool stream_open =
      this->transfer_sub_ == TRF_FULL ? this->current_data_index_ != 0 : this->transfer_sub_ == TRF_PARTIAL_DATA;
  if (stream_open) {
    this->disable();
    for (auto *cs : this->cs_pins_)
      cs->digital_write(true);
  }
  // The next transfer chooses its path again
  this->transfer_sub_ = TRF_START;
}

}  // namespace esphome::epaper_spi
