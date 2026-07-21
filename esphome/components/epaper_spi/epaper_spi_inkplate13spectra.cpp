#include "epaper_spi_inkplate13spectra.h"

#include <algorithm>

#include "esphome/core/log.h"

namespace esphome::epaper_spi {

static constexpr const char *const TAG = "epaper_spi.inkplate13spectra";

static constexpr uint8_t REG_DTM = 0x10;
static constexpr uint8_t REG_DRF = 0x12;
static constexpr uint8_t REG_PON = 0x04;
static constexpr uint8_t REG_POF = 0x02;
static constexpr uint8_t REG_PTLW = 0x83;   // partial window
static constexpr uint8_t REG_CMD66 = 0xF0;  // waveform select, required again before PTLW

static constexpr uint8_t CHIP_MASTER = 1;
static constexpr uint8_t CHIP_SLAVE = 2;
static constexpr uint8_t CHIP_BOTH = 3;

static constexpr uint8_t CMD66_V[] = {0x49, 0x55, 0x13, 0x5D, 0x05, 0x10};

// 4x4 window at physical origin -- sent to whichever chip the requested rectangle
// doesn't touch, so both chips still complete a full CMD66->PTLW->DTM cycle.
static constexpr uint8_t NULL_PTLW[9] = {0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x01, 0x01};

void EPaperInkplate13Spectra::setup() {
  EPaperBase::setup();
  this->rst_pin_->setup();
  this->pwr_en_pin_->setup();
  this->cs_m_pin_->setup();
  this->cs_s_pin_->setup();
  this->bs0_pin_->setup();
  this->bs1_pin_->setup();
}

void EPaperInkplate13Spectra::dump_config() {
  EPaperBase::dump_config();
  LOG_PIN("  RST Pin: ", this->rst_pin_);
  LOG_PIN("  PWR_EN Pin: ", this->pwr_en_pin_);
  LOG_PIN("  CS_M Pin: ", this->cs_m_pin_);
  LOG_PIN("  CS_S Pin: ", this->cs_s_pin_);
  LOG_PIN("  BS0 Pin: ", this->bs0_pin_);
  LOG_PIN("  BS1 Pin: ", this->bs1_pin_);
}

// A normal update() always does a full refresh; only display_partial() below does a
// sub-rectangle. transfer_sub_ decides which path transfer_data() takes, so it has to
// be set here, before the state machine starts.
void EPaperInkplate13Spectra::update() {
  this->partial_update_ = false;
  this->transfer_sub_ = TRF_MASTER;
  EPaperBase::update();
}

void EPaperInkplate13Spectra::display_partial(int x, int y, int w, int h) {
  if (this->state_ != EPaperState::IDLE) {
    ESP_LOGW(TAG, "display_partial(): skipped, display busy (state %s)", this->epaper_state_to_string_());
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

uint8_t EPaperInkplate13Spectra::color_to_index(Color color) {
  uint8_t r = color.r, g = color.g, b = color.b;
  if (r > 200 && g > 200 && b > 200) return 0x01;  // WHITE
  if (r < 50 && g < 50 && b < 50) return 0x00;      // BLACK
  if (r > 150 && g < 100 && b < 100) return 0x03;   // RED
  if (r < 100 && g > 150 && b < 100) return 0x06;   // GREEN
  if (r < 100 && g < 100 && b > 150) return 0x05;   // BLUE
  if (r > 150 && g > 150 && b < 100) return 0x02;   // YELLOW
  return 0x00;
}

void EPaperInkplate13Spectra::fill(Color color) {
  if (this->get_clipping().is_set()) {
    EPaperBase::fill(color);
    return;
  }
  uint8_t idx = color_to_index(color);
  this->buffer_.fill((uint8_t) ((idx << 4) | idx));
}

// 4bpp packed, 2 pixels per byte; high nibble holds the even-x pixel.
void HOT EPaperInkplate13Spectra::draw_pixel_at(int x, int y, Color color) {
  if (!this->rotate_coordinates_(x, y))
    return;
  uint8_t idx = color_to_index(color);
  uint32_t pos = (uint32_t) (x / 2) + (uint32_t) y * (this->width_ / 2);
  if (x % 2 == 0) {
    this->buffer_[pos] = (this->buffer_[pos] & 0x0F) | (idx << 4);
  } else {
    this->buffer_[pos] = (this->buffer_[pos] & 0xF0) | idx;
  }
}

void EPaperInkplate13Spectra::write_command_to_chip_(uint8_t cmd, const uint8_t *data, size_t len, uint8_t chip) {
  ESP_LOGD(TAG, "write_command_to_chip_: cmd=0x%02X len=%u chip=%u", cmd, (unsigned) len, (unsigned) chip);
  if (chip & CHIP_MASTER) this->cs_m_pin_->digital_write(false);
  if (chip & CHIP_SLAVE) this->cs_s_pin_->digital_write(false);

  this->enable();
  this->write_byte(cmd);
  if (len > 0 && data != nullptr)
    this->write_array(data, len);
  this->disable();

  if (chip & CHIP_MASTER) this->cs_m_pin_->digital_write(true);
  if (chip & CHIP_SLAVE) this->cs_s_pin_->digital_write(true);
}

void EPaperInkplate13Spectra::log_busy_state_(const char *where) {
  uint32_t now = millis();
  if (now - this->wait_log_ms_ < 1000) return;
  this->wait_log_ms_ = now;
  ESP_LOGD(TAG, "%s: waiting -- busy_pin raw digital_read=%d, is_idle_()=%d", where,
           (int) this->busy_pin_->digital_read(), (int) this->is_idle_());
}

void EPaperInkplate13Spectra::send_init_sequence_() {
  this->write_command_to_chip_(0x74, {0xC0, 0x1C, 0x1C, 0xCC, 0xCC, 0xCC, 0x15, 0x15, 0x55}, CHIP_MASTER);  // AN_TM
  this->write_command_to_chip_(0xF0, {0x49, 0x55, 0x13, 0x5D, 0x05, 0x10}, CHIP_BOTH);                      // CMD66
  this->write_command_to_chip_(0x00, {0xDF, 0x6B}, CHIP_BOTH);                                              // PSR
  this->write_command_to_chip_(0x30, {0x08}, CHIP_BOTH);                                                    // PLL
  this->write_command_to_chip_(0x50, {0xF7}, CHIP_BOTH);                                                    // CDI
  this->write_command_to_chip_(0x60, {0x03, 0x03}, CHIP_BOTH);                                              // TCON
  this->write_command_to_chip_(0x86, {0x10}, CHIP_BOTH);                                                    // AGID
  this->write_command_to_chip_(0xE3, {0x22}, CHIP_BOTH);                                                    // PWS
  this->write_command_to_chip_(0xE0, {0x01}, CHIP_BOTH);                                                    // CCSET
  this->write_command_to_chip_(0x61, {0x04, 0xB0, 0x03, 0x20}, CHIP_BOTH);                                   // TRES
  this->write_command_to_chip_(0x01, {0x0F, 0x00, 0x28, 0x2C, 0x28, 0x38}, CHIP_MASTER);                    // PWR
  this->write_command_to_chip_(0xB6, {0x07}, CHIP_MASTER);                                                  // EN_BUF
  this->write_command_to_chip_(0x06, {0xD8, 0x18}, CHIP_MASTER);                                            // BTST_P
  this->write_command_to_chip_(0xB7, {0x01}, CHIP_MASTER);                                                  // BOOST_VDDP_EN
  this->write_command_to_chip_(0x05, {0xD8, 0x18}, CHIP_MASTER);                                            // BTST_N
  this->write_command_to_chip_(0xB0, {0x01}, CHIP_MASTER);                                                  // BUCK_BOOST_VDDN
  this->write_command_to_chip_(0xB1, {0x02}, CHIP_MASTER);                                                  // TFT_VCOM_POWER
}

// Maps the requested logical (rotated) rectangle to physical panel columns/rows, then
// splits that physical window across the two chips (master: cols 0..599, slave: cols
// 600..1199). HRST/HRED are in half-column units, VRST/VRED in half-row units -- that's
// the panel's own addressing granularity, not something this code chose.
void EPaperInkplate13Spectra::compute_ptlw_params_() {
  this->ptlw_master_.needed = false;
  this->ptlw_slave_.needed = false;

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
  if (x + w > this->get_width()) w = this->get_width() - x;
  if (y + h > this->get_height()) h = this->get_height() - y;
  if (w <= 0 || h <= 0) return;

  const int16_t W = (int16_t) this->width_;   // physical panel width  = 1200
  const int16_t H = (int16_t) this->height_;  // physical panel height = 1600

  // Map logical coords to physical col/row by running both rectangle corners through
  // the same effective_transform_ (rotation + mirror_x/mirror_y) that draw_pixel_at()
  // uses via rotate_coordinates_(). This has to be the same transform as draw_pixel_at,
  // or the window pushed to the panel and the pixels actually drawn land in different
  // places -- rotation_ alone isn't enough on boards that also need a static mirror.
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
  if (col_end >= W) col_end = W - 1;
  if (row_start % 2) row_start--;
  if (row_start < 0) row_start = 0;
  if ((row_end + 1) % 2) row_end++;
  if (row_end >= H) row_end = H - 1;

  const int16_t HALF_W = W / 2;       // 600 physical cols per chip
  const int16_t HALF_BYTES = HALF_W / 2;  // 300 bytes per row per chip

  // Master chip handles physical cols 0..HALF_W-1.
  if (col_start < HALF_W) {
    int16_t lcs = col_start;
    int16_t lce = (col_end < HALF_W) ? col_end : (int16_t) (HALF_W - 1);
    uint16_t hrst = (uint16_t) lcs * 2;
    uint16_t hred = (uint16_t) (lce + 1) * 2 - 1;
    uint16_t vrst = (uint16_t) row_start / 2;
    uint16_t vred = (uint16_t) (row_end + 1) / 2 - 1;
    auto &p = this->ptlw_master_;
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

  // Slave chip handles physical cols HALF_W..W-1.
  if (col_end >= HALF_W) {
    int16_t lcs = (col_start >= HALF_W) ? (int16_t) (col_start - HALF_W) : 0;
    int16_t lce = col_end - HALF_W;
    uint16_t hrst = (uint16_t) lcs * 2;
    uint16_t hred = (uint16_t) (lce + 1) * 2 - 1;
    uint16_t vrst = (uint16_t) row_start / 2;
    uint16_t vred = (uint16_t) (row_end + 1) / 2 - 1;
    auto &p = this->ptlw_slave_;
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
    p.mem_col_off = HALF_BYTES + lcs / 2;
    p.row_start = row_start;
    p.row_end = row_end;
    p.needed = true;
  }
}

void EPaperInkplate13Spectra::set_all_pins_low_() {
  ESP_LOGD(TAG, "set_all_pins_low_()");
  GPIOPin *pins[] = {
      this->rst_pin_, this->dc_pin_, this->cs_m_pin_, this->cs_s_pin_,
      this->busy_pin_, this->pwr_en_pin_, this->bs0_pin_, this->bs1_pin_,
  };
  for (auto *p : pins) {
    p->pin_mode(gpio::FLAG_OUTPUT);
    p->digital_write(false);
  }
}

void EPaperInkplate13Spectra::set_io_pins_() {
  ESP_LOGD(TAG, "set_io_pins_()");
  this->rst_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->dc_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->cs_m_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->cs_s_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->busy_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  this->pwr_en_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->bs0_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->bs1_pin_->pin_mode(gpio::FLAG_OUTPUT);

  this->dc_pin_->digital_write(true);
  this->cs_m_pin_->digital_write(true);
  this->cs_s_pin_->digital_write(true);
  this->rst_pin_->digital_write(false);
  this->pwr_en_pin_->digital_write(false);
  this->bs0_pin_->digital_write(false);
  this->bs1_pin_->digital_write(true);
}

// Runs entirely inside RESET_END, which EPaperBase never busy-gates (see header), so the
// panel is fully powered before any busy-pin read happens. Each step sets reset_duration_
// to the delay this stage needs; the framework re-enters RESET_END after that many ms.
bool EPaperInkplate13Spectra::reset() {
  switch (this->reset_sub_) {
    case RST_PINS_LOW:
      ESP_LOGD(TAG, "reset(): RST_PINS_LOW");
      this->set_all_pins_low_();
      this->reset_duration_ = 50;
      this->reset_sub_ = RST_PINS_LOW_WAIT;
      return false;

    case RST_PINS_LOW_WAIT:
      ESP_LOGD(TAG, "reset(): RST_PINS_LOW_WAIT done -> set_io_pins_() + PWR_EN high");
      this->set_io_pins_();
      this->pwr_en_pin_->digital_write(true);
      this->reset_duration_ = 100;
      this->reset_sub_ = RST_IO_WAIT;
      return false;

    case RST_IO_WAIT:
      ESP_LOGD(TAG, "reset(): RST_IO_WAIT done -> RST low");
      this->rst_pin_->digital_write(false);
      this->reset_duration_ = 100;
      this->reset_sub_ = RST_LOW_WAIT;
      return false;

    case RST_LOW_WAIT:
      ESP_LOGD(TAG, "reset(): RST_LOW_WAIT done -> RST high");
      this->rst_pin_->digital_write(true);
      this->reset_duration_ = 100;
      this->reset_sub_ = RST_HIGH_WAIT;
      return false;

    case RST_HIGH_WAIT:
      ESP_LOGD(TAG, "reset(): RST_HIGH_WAIT done -> panel powered, entering INITIALISE");
      this->reset_sub_ = RST_DONE;
      return true;

    case RST_DONE:
      return true;
  }
  return false;
}

// Runs after reset() has already powered the panel on, so is_idle_() here reflects a
// real, live signal instead of an unpowered floating/pulled line.
bool EPaperInkplate13Spectra::initialise(bool partial) {
  switch (this->trf_init_sub_) {
    case INIT_SEND_SEQUENCE:
      ESP_LOGD(TAG, "initialise(): sending register init sequence + PON");
      this->send_init_sequence_();
      this->write_command_to_chip_(REG_PON, CHIP_BOTH);
      this->trf_init_sub_ = INIT_WAIT_PON;
      return false;

    case INIT_WAIT_PON:
      if (!this->is_idle_()) {
        this->log_busy_state_("initialise(): INIT_WAIT_PON");
        return false;
      }
      ESP_LOGD(TAG, "initialise(): INIT_WAIT_PON done -> INIT_DONE");
      this->trf_init_sub_ = INIT_DONE;
      return true;

    case INIT_DONE:
      return true;
  }
  return false;
}

// buffer_ is a SplitBuffer (not guaranteed contiguous), so each row is staged through a
// small stack buffer before write_array().
bool EPaperInkplate13Spectra::transfer_data() {
  const size_t rows = (size_t) this->height_;
  const size_t bytes_per_row = (size_t) this->width_ / 2;
  const size_t half = bytes_per_row / 2;  // bytes per row per chip
  uint8_t row_buf[512];  // half is 300 bytes for the 1200x1600 panel

  switch (this->transfer_sub_) {
    case TRF_MASTER: {
      if (this->transfer_row_ == 0) {
        ESP_LOGD(TAG, "transfer: master start");
        this->cs_m_pin_->digital_write(false);
        this->enable();
        this->write_byte(REG_DTM);
      }
      size_t end = std::min(this->transfer_row_ + ROWS_PER_CHUNK, rows);
      for (size_t i = this->transfer_row_; i < end; i++) {
        size_t row_off = i * bytes_per_row;
        for (size_t j = 0; j < half; j++) row_buf[j] = this->buffer_[row_off + j];
        this->write_array(row_buf, half);
      }
      this->transfer_row_ = end;
      if (this->transfer_row_ >= rows) {
        this->disable();
        this->cs_m_pin_->digital_write(true);
        this->transfer_row_ = 0;
        this->transfer_sub_ = TRF_WAIT_MASTER;
        ESP_LOGD(TAG, "transfer: master done");
      }
      return false;
    }

    case TRF_WAIT_MASTER:
      if (!this->is_idle_()) {
        this->log_busy_state_("transfer_data(): TRF_WAIT_MASTER");
        return false;
      }
      ESP_LOGD(TAG, "transfer_data(): TRF_WAIT_MASTER done -> TRF_SLAVE");
      this->transfer_sub_ = TRF_SLAVE;
      return false;

    case TRF_SLAVE: {
      if (this->transfer_row_ == 0) {
        ESP_LOGD(TAG, "transfer: slave start");
        this->cs_s_pin_->digital_write(false);
        this->enable();
        this->write_byte(REG_DTM);
      }
      size_t end = std::min(this->transfer_row_ + ROWS_PER_CHUNK, rows);
      for (size_t i = this->transfer_row_; i < end; i++) {
        size_t row_off = i * bytes_per_row + half;
        for (size_t j = 0; j < half; j++) row_buf[j] = this->buffer_[row_off + j];
        this->write_array(row_buf, half);
      }
      this->transfer_row_ = end;
      if (this->transfer_row_ >= rows) {
        this->disable();
        this->cs_s_pin_->digital_write(true);
        this->transfer_row_ = 0;
        this->transfer_sub_ = TRF_WAIT_SLAVE;
        ESP_LOGD(TAG, "transfer: slave done");
      }
      return false;
    }

    case TRF_WAIT_SLAVE:
      if (!this->is_idle_()) {
        this->log_busy_state_("transfer_data(): TRF_WAIT_SLAVE");
        return false;
      }
      ESP_LOGD(TAG, "transfer_data(): TRF_WAIT_SLAVE done -> TRF_DONE");
      this->transfer_sub_ = TRF_DONE;
      return true;

    // Null PTLW: 4-col x 4-row window at the physical origin of this chip, sent to a
    // chip the requested rectangle doesn't touch so it still completes CMD66->PTLW->DTM.
    case TRF_PARTIAL_SETUP_M: {
      this->write_command_to_chip_(REG_CMD66, CMD66_V, sizeof(CMD66_V), CHIP_MASTER);
      if (!this->ptlw_master_.needed) {
        this->write_command_to_chip_(REG_PTLW, NULL_PTLW, 9, CHIP_MASTER);
        this->cs_m_pin_->digital_write(false);
        this->enable();
        this->write_byte(REG_DTM);
        for (int r = 0; r < 4; r++) {
          size_t row_off = (size_t) r * bytes_per_row;
          for (size_t j = 0; j < 2; j++) row_buf[j] = this->buffer_[row_off + j];
          this->write_array(row_buf, 2);
        }
        this->disable();
        this->cs_m_pin_->digital_write(true);
        this->transfer_sub_ = TRF_PARTIAL_WAIT_M;
      } else {
        ESP_LOGD(TAG, "transfer: partial master CMD66+PTLW+DTM start");
        this->write_command_to_chip_(REG_PTLW, this->ptlw_master_.ptlw, 9, CHIP_MASTER);
        this->cs_m_pin_->digital_write(false);
        this->enable();
        this->write_byte(REG_DTM);
        this->transfer_row_ = (size_t) this->ptlw_master_.row_start;
        this->transfer_sub_ = TRF_PARTIAL_DATA_M;
      }
      return false;
    }

    case TRF_PARTIAL_DATA_M: {
      size_t end = std::min(this->transfer_row_ + ROWS_PER_CHUNK, (size_t) (this->ptlw_master_.row_end + 1));
      for (size_t i = this->transfer_row_; i < end; i++) {
        size_t row_off = i * bytes_per_row + (size_t) this->ptlw_master_.mem_col_off;
        for (int j = 0; j < this->ptlw_master_.bytes_per_row; j++) row_buf[j] = this->buffer_[row_off + j];
        this->write_array(row_buf, this->ptlw_master_.bytes_per_row);
      }
      this->transfer_row_ = end;
      if (this->transfer_row_ > (size_t) this->ptlw_master_.row_end) {
        this->disable();
        this->cs_m_pin_->digital_write(true);
        this->transfer_sub_ = TRF_PARTIAL_WAIT_M;
        ESP_LOGD(TAG, "transfer: partial master DTM done");
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
      this->write_command_to_chip_(REG_CMD66, CMD66_V, sizeof(CMD66_V), CHIP_SLAVE);
      if (!this->ptlw_slave_.needed) {
        this->write_command_to_chip_(REG_PTLW, NULL_PTLW, 9, CHIP_SLAVE);
        this->cs_s_pin_->digital_write(false);
        this->enable();
        this->write_byte(REG_DTM);
        for (int r = 0; r < 4; r++) {
          size_t row_off = (size_t) r * bytes_per_row + half;
          for (size_t j = 0; j < 2; j++) row_buf[j] = this->buffer_[row_off + j];
          this->write_array(row_buf, 2);
        }
        this->disable();
        this->cs_s_pin_->digital_write(true);
        this->transfer_sub_ = TRF_PARTIAL_WAIT_S;
      } else {
        ESP_LOGD(TAG, "transfer: partial slave CMD66+PTLW+DTM start");
        this->write_command_to_chip_(REG_PTLW, this->ptlw_slave_.ptlw, 9, CHIP_SLAVE);
        this->cs_s_pin_->digital_write(false);
        this->enable();
        this->write_byte(REG_DTM);
        this->transfer_row_ = (size_t) this->ptlw_slave_.row_start;
        this->transfer_sub_ = TRF_PARTIAL_DATA_S;
      }
      return false;
    }

    case TRF_PARTIAL_DATA_S: {
      size_t end = std::min(this->transfer_row_ + ROWS_PER_CHUNK, (size_t) (this->ptlw_slave_.row_end + 1));
      for (size_t i = this->transfer_row_; i < end; i++) {
        size_t row_off = i * bytes_per_row + (size_t) this->ptlw_slave_.mem_col_off;
        for (int j = 0; j < this->ptlw_slave_.bytes_per_row; j++) row_buf[j] = this->buffer_[row_off + j];
        this->write_array(row_buf, this->ptlw_slave_.bytes_per_row);
      }
      this->transfer_row_ = end;
      if (this->transfer_row_ > (size_t) this->ptlw_slave_.row_end) {
        this->disable();
        this->cs_s_pin_->digital_write(true);
        this->transfer_sub_ = TRF_PARTIAL_WAIT_S;
        ESP_LOGD(TAG, "transfer: partial slave DTM done");
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

// No-op: PON already went out in initialise(); nothing needs sending here.
void EPaperInkplate13Spectra::power_on() { ESP_LOGD(TAG, "power_on(): no-op (PON already sent in initialise())"); }

void EPaperInkplate13Spectra::refresh_screen(bool partial) {
  ESP_LOGD(TAG, "refresh_screen(): sending DRF");
  this->write_command_to_chip_(REG_DRF, {0x00}, CHIP_BOTH);
}

// EPaperBase's automatic busy-wait before DEEP_SLEEP covers the wait for POF to complete.
void EPaperInkplate13Spectra::power_off() {
  ESP_LOGD(TAG, "power_off(): sending POF");
  this->write_command_to_chip_(REG_POF, {0x00}, CHIP_BOTH);
}

void EPaperInkplate13Spectra::deep_sleep() {
  this->dc_pin_->pin_mode(gpio::FLAG_INPUT);
  this->cs_m_pin_->pin_mode(gpio::FLAG_INPUT);
  this->cs_s_pin_->pin_mode(gpio::FLAG_INPUT);
  // Keep the pullup so this pin reads a defined "idle" level at rest, since reset()
  // relies on that before it powers the panel back on next cycle.
  this->busy_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  this->pwr_en_pin_->pin_mode(gpio::FLAG_INPUT);
  this->pwr_en_pin_->digital_write(false);

  // RST intentionally NOT released to input -- held low for hardware deep sleep (~uA draw).
  this->rst_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->rst_pin_->digital_write(false);

  // Prime shared sub-state counters for the next cycle. transfer_sub_'s starting state
  // is decided in update()/display_partial() instead, since only they know at that point
  // whether the next cycle is a full or partial refresh.
  this->reset_sub_ = RST_PINS_LOW;
  this->trf_init_sub_ = INIT_SEND_SEQUENCE;
  this->transfer_row_ = 0;
  ESP_LOGD(TAG, "panel deep sleep");
}

}  // namespace esphome::epaper_spi
