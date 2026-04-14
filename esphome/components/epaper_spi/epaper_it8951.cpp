#include <array>
#include <cstring>
#include <string>

#include "epaper_it8951.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::epaper_spi {

static const char *const TAG = "epaper_spi.it8951";

// Maximum row buffer sizes for supported panel widths
static constexpr size_t IT8951_MAX_4BPP_ROW_BYTES = 2048 / 2;
static constexpr size_t IT8951_MAX_1BPP_ROW_BYTES = 2048 / 8;

// Short timeout for non-blocking LUTAFSR polling — limits blocking on HRDY race
static constexpr uint32_t POLL_BUSY_TIMEOUT = 50;  // ms

static uint16_t encode_uint16(uint8_t a, uint8_t b) { return static_cast<uint16_t>(a) << 8 | b; }

static UpdateModeE parse_update_mode(const std::string &mode) {
  if (mode == "DU" || mode == "fast")
    return UPDATE_MODE_DU;
  if (mode == "GC16" || mode == "full")
    return UPDATE_MODE_GC16;
  if (mode == "GL16")
    return UPDATE_MODE_GL16;
  if (mode == "GLR16")
    return UPDATE_MODE_GLR16;
  if (mode == "GLD16")
    return UPDATE_MODE_GLD16;
  if (mode == "DU4")
    return UPDATE_MODE_DU4;
  if (mode == "A2")
    return UPDATE_MODE_A2;
  if (mode == "INIT")
    return UPDATE_MODE_INIT;
  return UPDATE_MODE_NONE;
}

// --- IT8951 SPI protocol ---
// The IT8951 does NOT use a DC pin. Instead it uses 16-bit preamble words
// to distinguish command, write-data, and read-data transactions.

void EPaperIT8951::write_two_byte16_(uint16_t type, uint16_t cmd, uint32_t timeout) {
  this->wait_busy_(timeout);
  this->enable();
  this->write_byte16(type);
  this->wait_busy_(timeout);
  this->write_byte16(cmd);
  this->disable();
}

uint16_t EPaperIT8951::read_word_(uint32_t timeout) {
  this->wait_busy_(timeout);
  this->enable();
  this->write_byte16(IT8951_PACKET_TYPE_READ);
  this->wait_busy_(timeout);
  // dummy read
  this->write_byte16(0x0000);
  this->wait_busy_(timeout);

  // Read each byte individually via transfer_byte (full-duplex, sends 0 while
  // reading).  A single 2-byte transfer_array/read_array can lose the low byte
  // on ESP32 because ESP-IDF SPI DMA writes in 4-byte units, corrupting
  // unaligned or undersized RX buffers.
  const uint8_t hi = this->transfer_byte(0);
  const uint8_t lo = this->transfer_byte(0);

  this->disable();
  return encode_uint16(hi, lo);
}

void EPaperIT8951::read_words_(uint16_t *buf, uint32_t word_count) {
  this->wait_busy_();
  this->enable();
  this->write_byte16(IT8951_PACKET_TYPE_READ);
  this->wait_busy_();
  // dummy read
  this->write_byte16(0x0000);
  this->wait_busy_();

  for (uint32_t i = 0; i < word_count; i++) {
    // Read byte-by-byte — see read_word_() comment on DMA alignment.
    const uint8_t hi = this->transfer_byte(0);
    const uint8_t lo = this->transfer_byte(0);
    buf[i] = encode_uint16(hi, lo);
  }

  this->disable();
}

void EPaperIT8951::write_command_(uint16_t cmd, uint32_t timeout) {
  this->write_two_byte16_(IT8951_PACKET_TYPE_CMD, cmd, timeout);
}

void EPaperIT8951::write_word_(uint16_t data, uint32_t timeout) {
  this->write_two_byte16_(IT8951_PACKET_TYPE_WRITE, data, timeout);
}

void EPaperIT8951::write_reg_(uint16_t addr, uint16_t data) {
  this->write_command_(IT8951_TCON_REG_WR);
  this->wait_busy_();
  this->enable();
  this->write_byte16(IT8951_PACKET_TYPE_WRITE);
  this->wait_busy_();
  this->write_byte16(addr);
  this->wait_busy_();
  this->write_byte16(data);
  this->disable();
}

void EPaperIT8951::set_target_memory_addr_(uint16_t tar_addr_l, uint16_t tar_addr_h) {
  this->write_reg_(IT8951_LISAR + 2, tar_addr_h);
  this->write_reg_(IT8951_LISAR, tar_addr_l);
}

void EPaperIT8951::write_args_(uint16_t cmd, const uint16_t *args, uint16_t length) {
  this->write_command_(cmd);
  this->wait_busy_();
  this->enable();
  this->write_byte16(IT8951_PACKET_TYPE_WRITE);
  this->wait_busy_();
  for (uint16_t i = 0; i < length; i++) {
    this->write_byte16(args[i]);
  }
  this->disable();
}

// --- Display area management ---

void EPaperIT8951::set_area_(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  if (x == 0 && y == 0 && w == this->get_width_internal() && h == this->get_height_internal()) {
    uint16_t args[1];
    args[0] = (this->m_endian_type_ << 8 | this->m_pix_bpp_ << 4);
    this->write_args_(IT8951_TCON_LD_IMG, args, 1);
  } else {
    uint16_t args[5];
    args[0] = (this->m_endian_type_ << 8 | this->m_pix_bpp_ << 4);
    args[1] = x;
    args[2] = y;
    args[3] = w;
    args[4] = h;
    this->write_args_(IT8951_TCON_LD_IMG_AREA, args, 5);
  }
}

void EPaperIT8951::update_area_(uint16_t x, uint16_t y, uint16_t w, uint16_t h, UpdateModeE mode) {
  uint16_t args[7];
  args[0] = x;
  args[1] = y;
  args[2] = w;
  args[3] = h;
  args[4] = static_cast<uint16_t>(mode);
  args[5] = this->us_img_buf_addr_l_;
  args[6] = this->us_img_buf_addr_h_;
  this->write_args_(IT8951_I80_CMD_DPY_BUF_AREA, args, 7);
}

void EPaperIT8951::update_area_1bpp_(uint16_t x, uint16_t y, uint16_t w, uint16_t h, UpdateModeE mode, uint8_t bg_gray,
                                     uint8_t fg_gray) {
  // Enable 1bpp mode: set bit 2 in UP1SR high word
  this->write_command_(IT8951_TCON_REG_RD);
  this->write_word_(IT8951_UP1SR + 2);
  const uint16_t up1sr_high = this->read_word_();
  this->write_reg_(IT8951_UP1SR + 2, static_cast<uint16_t>(up1sr_high | (1U << 2)));
  // Set background/foreground gray levels for 1bpp rendering
  this->write_reg_(IT8951_BGVR, static_cast<uint16_t>((uint16_t(bg_gray) << 8) | fg_gray));

  this->update_area_(x, y, w, h, mode);
  // UP1SR bit 2 must remain set during the entire LUT refresh.
  // The caller is responsible for calling restore_1bpp_mode_() after the display finishes.
}

void EPaperIT8951::restore_1bpp_mode_() {
  // Clear bit 2 in UP1SR high word to restore normal (non-1bpp) display mode.
  // Must only be called after the LUT refresh is complete (LUTAFSR == 0).
  this->write_command_(IT8951_TCON_REG_RD);
  this->write_word_(IT8951_UP1SR + 2);
  const uint16_t up1sr_high = this->read_word_();
  this->write_reg_(IT8951_UP1SR + 2, static_cast<uint16_t>(up1sr_high & ~(1U << 2)));
}

// --- Busy/idle management ---

void EPaperIT8951::wait_busy_(uint32_t timeout) {
  if (this->busy_pin_ == nullptr)
    return;
  const uint32_t start_time = millis();
  // IT8951 busy pin: HIGH = ready, LOW = busy
  while (!this->busy_pin_->digital_read()) {
    if (millis() - start_time > timeout) {
      ESP_LOGE(TAG, "Busy pin timeout (%ums)", timeout);
      break;
    }
    App.feed_wdt();
    delay(1);
  }
}

bool EPaperIT8951::is_display_busy_() {
  // If HRDY is LOW, skip SPI — the controller can't accept commands right now.
  // The caller should retry after HRDY goes HIGH (via the waiting_for_idle_ gate).
  if (this->busy_pin_ != nullptr && !this->busy_pin_->digital_read()) {
    return true;
  }
  // Use a short wait_busy timeout (50ms) for the LUTAFSR read.  If HRDY drops
  // between the check above and the first SPI byte (TOCTOU race), this limits
  // the blocking to 3×50ms instead of 3×1000ms.  The caller will retry.
  this->write_command_(IT8951_TCON_REG_RD, POLL_BUSY_TIMEOUT);
  this->write_word_(IT8951_LUTAFSR, POLL_BUSY_TIMEOUT);
  return this->read_word_(POLL_BUSY_TIMEOUT) != 0;
}

// --- Device info and VCOM ---

void EPaperIT8951::get_dev_info_() {
  memset(&this->dev_info_, 0, sizeof(this->dev_info_));
  this->write_command_(IT8951_I80_CMD_GET_DEV_INFO);
  this->read_words_(reinterpret_cast<uint16_t *>(&this->dev_info_), sizeof(this->dev_info_) / sizeof(uint16_t));
  ESP_LOGI(TAG, "DevInfo: Panel %ux%u, ImgBuf 0x%04X%04X", this->dev_info_.panel_width, this->dev_info_.panel_height,
           this->dev_info_.img_buf_addr_h, this->dev_info_.img_buf_addr_l);
}

uint16_t EPaperIT8951::read_vcom_() {
  this->write_command_(IT8951_I80_CMD_VCOM);
  this->write_word_(IT8951_I80_CMD_VCOM_READ);
  const uint16_t vcom = this->read_word_();
  ESP_LOGI(TAG, "VCOM = %.02fV", static_cast<float>(vcom) / 1000.0f);
  return vcom;
}

void EPaperIT8951::write_vcom_(uint16_t vcom) {
  this->write_command_(IT8951_I80_CMD_VCOM);
  this->write_word_(IT8951_I80_CMD_VCOM_WRITE);
  this->write_word_(vcom);
}

// --- Component lifecycle ---

void EPaperIT8951::setup() {
  ESP_LOGCONFIG(TAG, "Setting up IT8951...");
  // Backup the configured data rate and use a low probe frequency for initial setup.
  // We'll switch to the configured speed after setup is complete.
  uint32_t configured_data_rate = this->data_rate_;
  this->data_rate_ = IT8951_SPI_PROBE_FREQUENCY;
  this->spi_setup();

  this->setup_pins_();

  // Hardware reset to ensure a clean state (IT8951 may be stuck from previous boot)
  this->reset();
  // Wait for the controller to finish its internal power-on init from SPI ROM
  this->wait_busy_();

  // Wake up the controller
  this->write_command_(IT8951_TCON_SYS_RUN);
  delay(10);  // Allow clocks to stabilize after SYS_RUN
  // Enable packed write mode
  this->write_reg_(IT8951_I80CPCR, 0x0001);

  // Read device info — retry up to 3 times since the first read after reset can return garbage
  bool dev_info_valid = false;
  for (int attempt = 0; attempt < 3; attempt++) {
    this->get_dev_info_();
    if (this->dev_info_.panel_width > 0 && this->dev_info_.panel_width <= 2048 && this->dev_info_.panel_height > 0 &&
        this->dev_info_.panel_height <= 2048 && this->dev_info_.panel_width != 0xFFFF &&
        this->dev_info_.panel_height != 0xFFFF) {
      dev_info_valid = true;
      break;
    }
    ESP_LOGW(TAG, "DevInfo attempt %d returned invalid data (W=%u H=%u), retrying...", attempt + 1,
             this->dev_info_.panel_width, this->dev_info_.panel_height);
    delay(100);  // NOLINT - give the controller more time
  }

  if (dev_info_valid) {
    this->width_ = this->dev_info_.panel_width;
    this->height_ = this->dev_info_.panel_height;
    this->row_width_ = static_cast<uint16_t>((static_cast<uint32_t>(this->width_) + 1) / 2);
    this->buffer_length_ = static_cast<size_t>(this->row_width_) * static_cast<size_t>(this->height_);
    this->us_img_buf_addr_l_ = this->dev_info_.img_buf_addr_l;
    this->us_img_buf_addr_h_ = this->dev_info_.img_buf_addr_h;
    ESP_LOGI(TAG, "Using DevInfo: %ux%u, ImgBuf 0x%04X%04X", this->width_, this->height_, this->us_img_buf_addr_h_,
             this->us_img_buf_addr_l_);
  } else {
    // Image buffer address is device-specific and only obtainable via DevInfo.
    // Without it we cannot safely write to the IT8951's memory.
    this->mark_failed(LOG_STR("Failed to read IT8951 device info — cannot determine image buffer address"));
    return;
  }

  // Set VCOM voltage
  const uint16_t vcom = this->read_vcom_();
  if (this->vcom_ != vcom) {
    ESP_LOGI(TAG, "Setting VCOM to %.02fV (was %.02fV)", static_cast<float>(this->vcom_) / 1000.0f,
             static_cast<float>(vcom) / 1000.0f);
    this->write_vcom_(this->vcom_);
    this->read_vcom_();
  }

  // Allocate frame buffer via base class SplitBuffer
  if (!this->init_buffer_(this->buffer_length_)) {
    this->mark_failed(LOG_STR("Failed to allocate display buffer"));
    return;
  }

  this->spi_teardown();
  this->set_data_rate(configured_data_rate);
  this->spi_setup();

  this->initialized_ = true;
  ESP_LOGCONFIG(TAG, "IT8951 setup complete.");
}

void EPaperIT8951::loop() {
  const auto now = millis();
  if (static_cast<int32_t>(now - this->delay_until_) < 0)
    return;
  if (this->waiting_for_idle_) {
    // IT8951 busy pin: HIGH = ready
    if (this->busy_pin_ == nullptr || this->busy_pin_->digital_read()) {
      this->waiting_for_idle_ = false;
    } else if (now - this->busy_wait_start_ > BUSY_TIMEOUT_MS) {
      ESP_LOGW(TAG, "Busy pin stuck LOW for %ums in state %d, recovering", now - this->busy_wait_start_,
               static_cast<int>(this->state_));
      this->recover_();
      return;
    } else {
      return;
    }
  }
  this->process_state_();
}

// --- State machine ---
// IT8951 flow: UPDATE -> [POWER_ON ->] TRANSFER_DATA -> REFRESH_SCREEN -> [POWER_OFF -> DEEP_SLEEP ->] IDLE

void EPaperIT8951::set_state_(EPaperState state, uint16_t delay) {
  this->state_ = state;
  this->delay_until_ = millis() + delay;
  bool should_wait = (state > EPaperState::SHOULD_WAIT);
  if (should_wait && !this->waiting_for_idle_) {
    this->busy_wait_start_ = millis();
  }
  this->waiting_for_idle_ = should_wait;
  if (state == EPaperState::IDLE) {
    if (this->update_pending_) {
      this->update_pending_ = false;
      this->queued_update_mode_ = this->pending_update_mode_;
      this->pending_mode_ = this->pending_update_mode_;
      this->update_started_at_ = millis();
      this->update_timing_active_ = true;
      this->state_ = EPaperState::UPDATE;
      return;
    }
    this->disable_loop();
  }
}

// --- 1bpp helpers ---

bool EPaperIT8951::framebuffer_is_binary_() {
  for (size_t i = 0; i < this->buffer_length_; i++) {
    const uint8_t byte = this->buffer_[i];
    const uint8_t hi = byte >> 4;
    const uint8_t lo = byte & 0x0F;
    if ((hi != 0x00 && hi != 0x0F) || (lo != 0x00 && lo != 0x0F)) {
      return false;
    }
  }
  return true;
}

uint8_t EPaperIT8951::get_pixel_nibble_(uint16_t x, uint16_t y) {
  const uint32_t index = static_cast<uint32_t>(y) * this->row_width_ + (static_cast<uint32_t>(x) >> 1);
  const uint8_t byte = this->buffer_[index];
  if ((x & 1U) == 0) {
    return byte >> 4;
  }
  return byte & 0x0F;
}

// --- Transfer ---

bool EPaperIT8951::prepare_transfer_(UpdateModeE &mode) {
  this->partial_update_++;
  if (this->full_update_every_ > 0 && this->partial_update_ >= this->full_update_every_) {
    this->partial_update_ = 0;
    mode = UPDATE_MODE_GC16;
    this->x_low_ = 0;
    this->y_low_ = 0;
    this->x_high_ = this->get_width_internal();
    this->y_high_ = this->get_height_internal();
  } else {
    // IT8951 partial write requires x and width to be multiples of 4 pixels
    this->x_low_ &= 0xFFFC;
    uint16_t temp_max = this->x_high_ > 0 ? static_cast<uint16_t>(this->x_high_ - 1) : 0;
    temp_max = static_cast<uint16_t>(temp_max | 0x0003);
    if (temp_max >= this->get_width_internal())
      temp_max = this->get_width_internal() - 1;
    this->x_high_ = static_cast<uint16_t>(temp_max + 1);
  }

  if (this->x_high_ <= this->x_low_ || this->y_high_ <= this->y_low_)
    return false;

  const uint16_t x = static_cast<uint16_t>(this->x_low_);
  const uint16_t y = static_cast<uint16_t>(this->y_low_);
  const uint16_t width = static_cast<uint16_t>(this->x_high_ - this->x_low_);
  const uint16_t height = static_cast<uint16_t>(this->y_high_ - this->y_low_);

  if (x >= this->get_width_internal() || y >= this->get_height_internal()) {
    ESP_LOGE(TAG, "Position (%d, %d) out of bounds", x, y);
    // Reset dirty region before returning to prevent reusing invalid coordinates
    this->x_low_ = this->width_;
    this->x_high_ = 0;
    this->y_low_ = this->height_;
    this->y_high_ = 0;
    return false;
  }
  if ((x + width) > this->get_width_internal() || (y + height) > this->get_height_internal()) {
    ESP_LOGE(TAG, "Dimension (%d, %d) out of bounds", x + width, y + height);
    // Reset dirty region before returning to prevent reusing invalid coordinates
    this->x_low_ = this->width_;
    this->x_high_ = 0;
    this->y_low_ = this->height_;
    this->y_high_ = 0;
    return false;
  }

  this->pending_x_ = x;
  this->pending_y_ = y;
  this->pending_w_ = width;
  this->pending_h_ = height;
  this->transfer_row_ = 0;
  this->use_1bpp_ = this->force_1bpp_ || this->framebuffer_is_binary_();

  // Reset dirty region
  this->x_low_ = this->width_;
  this->x_high_ = 0;
  this->y_low_ = this->height_;
  this->y_high_ = 0;

  ESP_LOGD(TAG, "Transfer: %dx%d @ %d,%d mode=%d (%s)", width, height, x, y, static_cast<int>(mode),
           this->use_1bpp_ ? "1bpp" : "4bpp");
  return true;
}

bool EPaperIT8951::transfer_row_data_1bpp_() {
  const uint16_t area_w = this->pending_w_;
  const uint16_t area_h = this->pending_h_;
  const uint16_t words_per_row = static_cast<uint16_t>((area_w + 15) / 16);

  if (words_per_row * 2 > IT8951_MAX_1BPP_ROW_BYTES) {
    ESP_LOGE(TAG, "1bpp row buffer too small for %u-word transfer", words_per_row);
    return true;
  }

  const uint32_t start_time = millis();
  // Each word holds 16 pixels, packed LSB-first to match 1bpp layout.
  std::array<uint16_t, IT8951_MAX_1BPP_ROW_BYTES / 2> row_words{};

  // Set up LD_IMG_AREA for this chunk (remaining rows from transfer_row_ onward).
  // Must be done every chunk — the IT8951 resets its write pointer when CS goes HIGH.
  // L_ENDIAN: matches Waveshare reference.  The IT8951 swaps bytes within each 16-bit
  // word, which together with the LSB-first bit packing produces the correct pixel order.
  this->m_endian_type_ = IT8951_LDIMG_L_ENDIAN;
  this->m_pix_bpp_ = IT8951_8BPP;
  if (this->transfer_row_ == 0) {
    this->set_target_memory_addr_(this->us_img_buf_addr_l_, this->us_img_buf_addr_h_);
  }
  const uint16_t remaining_h = static_cast<uint16_t>(area_h - this->transfer_row_);
  uint16_t bpp_args[5];
  bpp_args[0] = (this->m_endian_type_ << 8 | this->m_pix_bpp_ << 4);
  bpp_args[1] = static_cast<uint16_t>(this->pending_x_ / 8);
  bpp_args[2] = static_cast<uint16_t>(this->pending_y_ + this->transfer_row_);
  bpp_args[3] = static_cast<uint16_t>(area_w / 8);
  bpp_args[4] = remaining_h;
  this->write_args_(IT8951_TCON_LD_IMG_AREA, bpp_args, 5);

  // Match the 4bpp path: wait_busy before CS assert, no wait after preamble.
  // In packed write mode, data must flow continuously after the preamble.
  this->wait_busy_();
  this->enable();
  this->write_byte16(IT8951_PACKET_TYPE_WRITE);

  while (this->transfer_row_ < area_h) {
    row_words.fill(0);

    const uint16_t row_y = static_cast<uint16_t>(this->pending_y_ + this->transfer_row_);
    for (uint16_t x = 0; x < area_w; x++) {
      const uint8_t nibble = this->get_pixel_nibble_(static_cast<uint16_t>(this->pending_x_ + x), row_y);
      // Nibble <= 0x07 is dark (closer to black), set the bit.
      // Pack into 16-bit words: bit 0 = leftmost pixel of each group of 16.
      if (nibble <= 0x07) {
        row_words[x / 16] |= static_cast<uint16_t>(1U << (x & 0x0F));
      }
    }

    // write_array sends raw LE bytes, but the IT8951 L_ENDIAN + MSB_FIRST SPI
    // expects the same byte order as write_byte16 (which byte-swaps each word).
    for (uint16_t i = 0; i < words_per_row; i++) {
      row_words[i] = byteswap(row_words[i]);
    }
    this->write_array(reinterpret_cast<const uint8_t *>(row_words.data()), words_per_row * 2);
    this->transfer_row_++;

    if (millis() - start_time >= MAX_TRANSFER_TIME) {
      break;
    }
  }

  this->disable();
  // Send LD_IMG_END after every chunk (not just the last) — the IT8951 needs it
  // to commit the data before the next LD_IMG_AREA command.
  this->write_command_(IT8951_TCON_LD_IMG_END);

  return this->transfer_row_ >= area_h;
}

bool EPaperIT8951::transfer_row_data_() {
  if (this->use_1bpp_) {
    return this->transfer_row_data_1bpp_();
  }

  this->m_endian_type_ = IT8951_LDIMG_B_ENDIAN;
  this->m_pix_bpp_ = IT8951_4BPP;
  const uint32_t start_time = millis();

  const uint16_t area_x = this->pending_x_;
  const uint16_t area_y = this->pending_y_;
  const uint16_t area_w = this->pending_w_;
  const uint16_t area_h = this->pending_h_;

  if (this->transfer_row_ == 0) {
    this->set_target_memory_addr_(this->us_img_buf_addr_l_, this->us_img_buf_addr_h_);
  }

  // Set up LD_IMG_AREA for this chunk (remaining rows from transfer_row_ onward).
  // Must be done every chunk — the IT8951 resets its write pointer when CS goes HIGH.
  const uint16_t remaining_h = static_cast<uint16_t>(area_h - this->transfer_row_);
  this->set_area_(area_x, static_cast<uint16_t>(area_y + this->transfer_row_), area_w, remaining_h);

  this->wait_busy_();
  this->enable();
  this->write_byte16(IT8951_PACKET_TYPE_WRITE);

  const bool full_width = (area_x == 0 && area_w == this->get_width_internal());
  const uint16_t bytes_per_row = full_width ? this->row_width_ : static_cast<uint16_t>(area_w >> 1);

  if (bytes_per_row > IT8951_MAX_4BPP_ROW_BYTES) {
    ESP_LOGE(TAG, "4bpp row buffer too small for %u-byte transfer", bytes_per_row);
    this->disable();
    return true;
  }

  std::array<uint8_t, IT8951_MAX_4BPP_ROW_BYTES> row_buffer{};

  while (this->transfer_row_ < area_h) {
    const uint32_t row_y = area_y + this->transfer_row_;
    const uint32_t offset = row_y * this->row_width_ + (full_width ? 0 : (area_x >> 1));

    for (uint16_t i = 0; i < bytes_per_row; i++) {
      row_buffer[i] = this->buffer_[offset + i];
    }
    this->write_array(row_buffer.data(), bytes_per_row);
    this->transfer_row_++;

    if (millis() - start_time >= MAX_TRANSFER_TIME) {
      break;
    }
  }

  this->disable();

  // Send LD_IMG_END after every chunk (not just the last) — the IT8951 needs it
  // to commit the data before the next LD_IMG_AREA command.
  this->write_command_(IT8951_TCON_LD_IMG_END);

  return this->transfer_row_ >= area_h;
}

bool EPaperIT8951::transfer_data() { return this->transfer_row_data_(); }

// Not called — IT8951 uses process_state_() exclusively. Kept for interface compliance.
void EPaperIT8951::refresh_screen(bool partial) {}

void EPaperIT8951::power_on() { this->write_command_(IT8951_TCON_SYS_RUN); }
void EPaperIT8951::power_off() { this->write_command_(IT8951_TCON_SLEEP); }
void EPaperIT8951::deep_sleep() { this->write_command_(IT8951_TCON_SLEEP); }

bool EPaperIT8951::reset() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(true);
    this->reset_pin_->digital_write(false);
    delay(this->reset_duration_);
    this->reset_pin_->digital_write(true);
    delay(100);  // NOLINT
  }
  return true;  // single-step reset, no RESET_END needed
}

void EPaperIT8951::recover_() {
  ESP_LOGW(TAG, "Recovering: resetting controller (was in state %d)", static_cast<int>(this->state_));
  this->waiting_for_idle_ = false;
  this->update_timing_active_ = false;
  this->update_pending_ = false;
  this->pending_1bpp_restore_ = false;
  this->transfer_row_ = 0;

  // Hardware reset the IT8951
  this->reset();
  // Re-initialize: wake up and enable packed write mode
  this->write_command_(IT8951_TCON_SYS_RUN);
  this->write_reg_(IT8951_I80CPCR, 0x0001);

  // Reset dirty region to full screen so next update redraws everything
  this->x_low_ = 0;
  this->y_low_ = 0;
  this->x_high_ = this->get_width_internal();
  this->y_high_ = this->get_height_internal();

  this->state_ = EPaperState::IDLE;
  this->disable_loop();
  ESP_LOGW(TAG, "Recovery complete, triggering full redraw");

  // Force a full redraw immediately so the display doesn't stay stale
  this->update_pending_ = true;
  this->set_state_(EPaperState::UPDATE);
}

void EPaperIT8951::process_state_() {
  switch (this->state_) {
    case EPaperState::IDLE:
      this->disable_loop();
      break;

    case EPaperState::UPDATE:
      this->do_update_();
      if (!this->prepare_transfer_(this->pending_mode_)) {
        this->update_timing_active_ = false;
        this->set_state_(EPaperState::IDLE);
        break;
      }
      this->set_state_(this->sleep_when_done_ ? EPaperState::POWER_ON : EPaperState::TRANSFER_DATA);
      break;

    case EPaperState::POWER_ON:
      this->power_on();
      this->set_state_(EPaperState::TRANSFER_DATA);
      break;

    case EPaperState::TRANSFER_DATA:
      if (this->transfer_row_data_()) {
        this->set_state_(EPaperState::REFRESH_SCREEN);
      }
      break;

    case EPaperState::REFRESH_SCREEN:
      if (this->queued_update_mode_ == UPDATE_MODE_NONE) {
        this->set_state_(EPaperState::IDLE);
        break;
      }
      if (this->is_display_busy_()) {
        if (millis() - this->busy_wait_start_ > BUSY_TIMEOUT_MS) {
          ESP_LOGW(TAG, "LUT busy timeout after %ums, recovering", millis() - this->busy_wait_start_);
          this->recover_();
          return;
        }
        this->waiting_for_idle_ = true;  // Re-arm HRDY gate before retrying
        return;                          // LUT still busy, retry next loop
      }
      if (this->use_1bpp_) {
        this->update_area_1bpp_(this->pending_x_, this->pending_y_, this->pending_w_, this->pending_h_,
                                this->queued_update_mode_, 0xFF, 0x00);
      } else {
        this->update_area_(this->pending_x_, this->pending_y_, this->pending_w_, this->pending_h_,
                           this->queued_update_mode_);
      }
      // After sending the display command, the LUT engine will run.
      // For 1bpp mode, we must wait for the refresh to finish before restoring UP1SR.
      // Use a 100ms delay to skip the brief HRDY HIGH pulse that occurs right after
      // command acceptance (before HRDY drops LOW for the actual LUT refresh).
      // The HRDY gate in loop() then waits for the real completion.
      this->pending_1bpp_restore_ = this->use_1bpp_;
      this->set_state_(EPaperState::POWER_OFF, 100);
      break;

    case EPaperState::POWER_OFF:
      // Wait for the LUT engine to finish before restoring 1bpp mode or sleeping.
      // HRDY HIGH only means SPI is ready, NOT that the LUT is done.
      // is_display_busy_() checks HRDY first (no SPI if LOW), then reads LUTAFSR.
      if (this->pending_1bpp_restore_) {
        if (this->is_display_busy_()) {
          if (millis() - this->busy_wait_start_ > BUSY_TIMEOUT_MS) {
            ESP_LOGW(TAG, "LUT busy timeout in POWER_OFF after %ums, recovering", millis() - this->busy_wait_start_);
            this->pending_1bpp_restore_ = false;
            this->recover_();
            return;
          }
          // LUT still running — check again in 100ms (no blocking, no HRDY re-arm)
          this->delay_until_ = millis() + 100;
          return;
        }
        this->restore_1bpp_mode_();
        this->pending_1bpp_restore_ = false;
      }
      if (this->update_timing_active_) {
        ESP_LOGD(TAG, "Update took %ums (mode=%d area=%ux%u@%u,%u)", millis() - this->update_started_at_,
                 static_cast<int>(this->queued_update_mode_), this->pending_w_, this->pending_h_, this->pending_x_,
                 this->pending_y_);
        this->update_timing_active_ = false;
      }
      if (this->sleep_when_done_) {
        this->set_state_(EPaperState::DEEP_SLEEP);
      } else {
        this->set_state_(EPaperState::IDLE);
      }
      break;

    case EPaperState::DEEP_SLEEP:
      this->deep_sleep();
      this->set_state_(EPaperState::IDLE);
      break;

    default:
      ESP_LOGE(TAG, "Unhandled state %d", static_cast<int>(this->state_));
      this->set_state_(EPaperState::IDLE);
      break;
  }
}

// --- Public update methods ---

void EPaperIT8951::start_update_(UpdateModeE hw_mode) {
  if (this->state_ == EPaperState::IDLE) {
    this->update_started_at_ = millis();
    this->update_timing_active_ = true;
    this->queued_update_mode_ = hw_mode;
    this->enable_loop();
    this->set_state_(EPaperState::UPDATE);
  } else {
    this->update_pending_ = true;
    this->pending_update_mode_ = hw_mode;
  }
}

void EPaperIT8951::update_mode(const std::string &mode) {
  if (!this->is_ready() || !this->initialized_)
    return;

  UpdateModeE hw_mode = parse_update_mode(mode);
  if (hw_mode == UPDATE_MODE_NONE) {
    ESP_LOGW(TAG, "Unknown update mode '%s'", mode.c_str());
    return;
  }

  this->start_update_(hw_mode);
}

void EPaperIT8951::update() {
  if (!this->is_ready() || !this->initialized_)
    return;

  // If a default mode is configured, use it; otherwise fall back to GC16.
  if (!this->get_update_mode().empty()) {
    this->update_mode(this->get_update_mode());
    return;
  }

  this->start_update_(UPDATE_MODE_GC16);
}

// --- Color conversion ---

uint8_t EPaperIT8951::color_to_nibble_(const Color &color) const {
  // Fast paths for common colors
  if (color.raw_32 == 0)
    return 0x00;  // COLOR_OFF -> white (lightest)
  if (color.raw_32 == 0xFFFFFFFF)
    return 0x0F;  // COLOR_ON -> black (darkest)

  // Direct nibble path: if only red channel has a value 0-15, use it directly
  if (color.g == 0 && color.b == 0 && color.w == 0 && color.r <= 0x0F)
    return color.r;

  // General RGB to 4-bit grayscale conversion
  uint16_t gray = static_cast<uint16_t>(color.r) + color.g + color.b;
  gray /= 3;
  if (color.w > gray)
    gray = color.w;
  uint8_t nibble = static_cast<uint8_t>((gray + 8) >> 4);
  if (nibble > 0x0F)
    nibble = 0x0F;
  return nibble;
}

// --- Drawing ---

void EPaperIT8951::fill(Color color) {
  if (this->get_clipping().is_set()) {
    esphome::epaper_spi::EPaperBase::fill(color);
    return;
  }

  uint8_t packed_color = this->color_to_nibble_(color);
  if (!this->reversed_)
    packed_color = 0x0F - packed_color;
  const uint8_t fill_byte = static_cast<uint8_t>((packed_color << 4) | packed_color);

  this->buffer_.fill(fill_byte);
  this->x_high_ = this->get_width_internal();
  this->y_high_ = this->get_height_internal();
  this->x_low_ = 0;
  this->y_low_ = 0;
}

void HOT EPaperIT8951::draw_pixel_at(int x, int y, Color color) {
  if (!this->rotate_coordinates_(x, y))
    return;

  uint8_t internal_color = this->color_to_nibble_(color) & 0x0F;
  if (!this->reversed_)
    internal_color = 0x0F - internal_color;

  const uint32_t index = static_cast<uint32_t>(y) * this->row_width_ + (static_cast<uint32_t>(x) >> 1);

  uint8_t buf = this->buffer_[index];
  if (x & 0x1) {
    buf = (buf & 0xF0) | internal_color;
  } else {
    buf = (buf & 0x0F) | (internal_color << 4);
  }
  this->buffer_[index] = buf;
}

// --- Config dump ---

void EPaperIT8951::dump_config() {
  LOG_DISPLAY("", "IT8951 E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Dimensions: %dx%d", this->get_width_internal(), this->get_height_internal());
  ESP_LOGCONFIG(TAG, "  Buffer: %u bytes in %u segment(s)", static_cast<unsigned>(this->buffer_length_),
                static_cast<unsigned>(this->buffer_.get_buffer_count()));
  ESP_LOGCONFIG(TAG, "  Image buffer addr: 0x%04X%04X", this->us_img_buf_addr_h_, this->us_img_buf_addr_l_);
  ESP_LOGCONFIG(TAG, "  Sleep when done: %s", YESNO(this->sleep_when_done_));
  ESP_LOGCONFIG(TAG, "  Full update every: %u", this->full_update_every_);
  ESP_LOGCONFIG(TAG, "  Reversed colors: %s", YESNO(this->reversed_));
  ESP_LOGCONFIG(TAG, "  Force 1bpp: %s", YESNO(this->force_1bpp_));
  ESP_LOGCONFIG(TAG, "  Reset duration: %ums", this->reset_duration_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace esphome::epaper_spi
