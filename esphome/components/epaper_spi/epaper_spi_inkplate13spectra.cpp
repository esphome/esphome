#include "epaper_spi_inkplate13spectra.h"

#include <algorithm>

#include "esphome/core/log.h"

namespace esphome::epaper_spi {

static constexpr const char *const TAG = "epaper_spi.inkplate13spectra";

static constexpr uint8_t REG_DTM = 0x10;
static constexpr uint8_t REG_DRF = 0x12;
static constexpr uint8_t REG_PON = 0x04;
static constexpr uint8_t REG_POF = 0x02;

static constexpr uint8_t CHIP_MASTER = 1;
static constexpr uint8_t CHIP_SLAVE = 2;
static constexpr uint8_t CHIP_BOTH = 3;

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

  // Prime sub-state counters for the next cycle's reset()/initialise()/transfer_data().
  this->reset_sub_ = RST_PINS_LOW;
  this->trf_init_sub_ = INIT_SEND_SEQUENCE;
  this->transfer_sub_ = TRF_MASTER;
  this->transfer_row_ = 0;
  ESP_LOGD(TAG, "panel deep sleep");
}

}  // namespace esphome::epaper_spi
