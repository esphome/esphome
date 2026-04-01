#include "epaper_it8951e.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome::epaper_spi {

static const char *const TAG = "epaper_spi.it8951e";

static uint16_t encode_uint16(uint8_t a, uint8_t b) { return static_cast<uint16_t>(a) << 8 | b; }

// --- IT8951E SPI protocol ---
// The IT8951E does NOT use a DC pin. Instead it uses 16-bit preamble words
// to distinguish command, write-data, and read-data transactions.

void EPaperIT8951E::write_two_byte16_(uint16_t type, uint16_t cmd) {
  this->wait_busy_();
  this->enable();
  this->write_byte16(type);
  this->wait_busy_();
  this->write_byte16(cmd);
  this->disable();
}

uint16_t EPaperIT8951E::read_word_() {
  this->wait_busy_();
  this->enable();
  this->write_byte16(IT8951_PACKET_TYPE_READ);
  this->wait_busy_();
  // dummy read
  this->write_byte16(0x0000);
  this->wait_busy_();

  uint8_t recv[2];
  this->read_array(recv, sizeof(recv));
  uint16_t word = encode_uint16(recv[0], recv[1]);

  this->disable();
  return word;
}

void EPaperIT8951E::write_command_(uint16_t cmd) {
  this->write_two_byte16_(IT8951_PACKET_TYPE_CMD, cmd);
}

void EPaperIT8951E::write_word_(uint16_t cmd) {
  this->write_two_byte16_(IT8951_PACKET_TYPE_WRITE, cmd);
}

void EPaperIT8951E::write_reg_(uint16_t addr, uint16_t data) {
  this->write_command_(IT8951_TCON_REG_WR);
  this->wait_busy_();
  this->enable();
  // Preamble byte (not 16-bit) per IT8951 spec
  this->write_byte(IT8951_PACKET_TYPE_WRITE);
  this->wait_busy_();
  this->write_byte16(addr);
  this->wait_busy_();
  this->write_byte16(data);
  this->disable();
}

void EPaperIT8951E::set_target_memory_addr_(uint16_t tar_addr_l, uint16_t tar_addr_h) {
  this->write_reg_(IT8951_LISAR + 2, tar_addr_h);
  this->write_reg_(IT8951_LISAR, tar_addr_l);
}

void EPaperIT8951E::write_args_(uint16_t cmd, uint16_t *args, uint16_t length) {
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

void EPaperIT8951E::set_area_(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
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

void EPaperIT8951E::wait_busy_(uint32_t timeout) {
  if (this->busy_pin_ == nullptr)
    return;
  const uint32_t start_time = millis();
  // IT8951E busy pin: HIGH = ready, LOW = busy
  while (!this->busy_pin_->digital_read()) {
    if (millis() - start_time > timeout) {
      ESP_LOGE(TAG, "Busy pin timeout (%ums)", timeout);
      break;
    }
  }
}

bool EPaperIT8951E::is_display_busy_() {
  this->write_command_(IT8951_TCON_REG_RD);
  this->write_word_(IT8951_LUTAFSR);
  return this->read_word_() != 0;
}

void EPaperIT8951E::update_area_(uint16_t x, uint16_t y, uint16_t w, uint16_t h, update_mode_e mode) {
  uint16_t args[7];
  args[0] = x;
  args[1] = y;
  args[2] = w;
  args[3] = h;
  args[4] = static_cast<uint16_t>(mode);
  args[5] = this->usImgBufAddrL_;
  args[6] = this->usImgBufAddrH_;
  this->write_args_(IT8951_I80_CMD_DPY_BUF_AREA, args, 7);
}

bool EPaperIT8951E::reset() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(true);
    this->reset_pin_->digital_write(false);
    delay(this->reset_duration_);
    this->reset_pin_->digital_write(true);
    delay(100);
  }
  return true;  // single-step reset, no RESET_END needed
}

uint16_t EPaperIT8951E::get_vcom_() {
  this->write_command_(IT8951_I80_CMD_VCOM);
  this->write_word_(IT8951_I80_CMD_VCOM_READ);
  const uint16_t vcom = this->read_word_();
  ESP_LOGI(TAG, "VCOM = %.02fV", static_cast<float>(vcom) / 1000.0f);
  return vcom;
}

void EPaperIT8951E::set_vcom_(uint16_t vcom) {
  this->write_command_(IT8951_I80_CMD_VCOM);
  this->write_word_(IT8951_I80_CMD_VCOM_WRITE);
  this->write_word_(vcom);
}

// --- Component lifecycle ---

void EPaperIT8951E::setup() {
  ESP_LOGCONFIG(TAG, "Setting up IT8951E...");
  this->spi_setup();

  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset();
  }

  if (this->busy_pin_ != nullptr) {
    this->busy_pin_->setup();
  }

  // Wake up the controller
  this->write_command_(IT8951_TCON_SYS_RUN);
  // Enable packed write mode
  this->write_reg_(IT8951_I80CPCR, 0x0001);

  // Set VCOM to default (-2.30V)
  const uint16_t vcom = this->get_vcom_();
  if (IT8951_DEFAULT_VCOM != vcom) {
    this->set_vcom_(IT8951_DEFAULT_VCOM);
    this->get_vcom_();
  }

  // Allocate frame buffer via base class SplitBuffer
  if (!this->init_buffer_(this->buffer_length_)) {
    this->mark_failed(LOG_STR("Failed to allocate display buffer"));
    return;
  }

  this->initialized_ = true;
  ESP_LOGCONFIG(TAG, "IT8951E setup complete.");
}

void EPaperIT8951E::loop() {
  const auto now = millis();
  if (static_cast<int32_t>(now - this->delay_until_) < 0)
    return;
  if (this->waiting_for_idle_) {
    // IT8951E busy pin: HIGH = ready
    if (this->busy_pin_ == nullptr || this->busy_pin_->digital_read()) {
      this->waiting_for_idle_ = false;
    } else {
      return;
    }
  }
  this->process_state_();
}

// --- State machine ---
// IT8951E flow: UPDATE -> [POWER_ON ->] TRANSFER_DATA -> REFRESH_SCREEN -> [POWER_OFF -> DEEP_SLEEP ->] IDLE

void EPaperIT8951E::set_state_(EPaperState state, uint16_t delay) {
  this->state_ = state;
  this->delay_until_ = millis() + delay;
  this->waiting_for_idle_ = (state > EPaperState::SHOULD_WAIT);
  if (state == EPaperState::IDLE) {
    if (this->update_pending_) {
      this->update_pending_ = false;
      this->pending_mode_ = this->queued_update_mode_;
      this->update_started_at_ = millis();
      this->update_timing_active_ = true;
      this->state_ = EPaperState::UPDATE;
      return;
    }
    this->disable_loop();
  }
}

bool EPaperIT8951E::prepare_transfer_(update_mode_e &mode) {
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
    return false;
  }
  if ((x + width) > this->get_width_internal() || (y + height) > this->get_height_internal()) {
    ESP_LOGE(TAG, "Dimension (%d, %d) out of bounds", x + width, y + height);
    return false;
  }

  this->pending_x_ = x;
  this->pending_y_ = y;
  this->pending_w_ = width;
  this->pending_h_ = height;
  this->transfer_row_ = 0;

  // Reset dirty region
  this->x_low_ = this->width_;
  this->x_high_ = 0;
  this->y_low_ = this->height_;
  this->y_high_ = 0;

  ESP_LOGD(TAG, "Transfer: %dx%d @ %d,%d mode=%d", width, height, x, y, static_cast<int>(mode));
  return true;
}

bool EPaperIT8951E::transfer_row_data_() {
  this->m_endian_type_ = IT8951_LDIMG_B_ENDIAN;
  this->m_pix_bpp_ = IT8951_4BPP;
  const uint32_t start_time = millis();

  if (this->transfer_row_ == 0) {
    this->set_target_memory_addr_(this->usImgBufAddrL_, this->usImgBufAddrH_);
  }

  const uint16_t area_x = this->pending_x_;
  const uint16_t area_y = this->pending_y_;
  const uint16_t area_w = this->pending_w_;
  const uint16_t area_h = this->pending_h_;

  const uint16_t remaining_h = area_h - this->transfer_row_;
  this->set_area_(area_x, area_y + this->transfer_row_, area_w, remaining_h);
  this->enable();
  this->write_byte16(IT8951_PACKET_TYPE_WRITE);

  // SplitBuffer may be non-contiguous, so copy row data to a temp buffer
  // before sending via write_array().
  const bool full_width = (area_x == 0 && area_w == this->get_width_internal());
  const uint16_t bytes_per_row = full_width ? this->row_width_ : static_cast<uint16_t>(area_w >> 1);

  // Stack buffer for one row. 960/2 = 480 bytes max for M5EPD.
  uint8_t row_buf[480];

  while (this->transfer_row_ < area_h) {
    const uint32_t row_y = area_y + this->transfer_row_;
    const uint32_t offset = row_y * this->row_width_ + (full_width ? 0 : (area_x >> 1));

    for (uint16_t i = 0; i < bytes_per_row; i++) {
      row_buf[i] = this->buffer_[offset + i];
    }
    this->write_array(row_buf, bytes_per_row);
    this->transfer_row_++;

    if (millis() - start_time >= MAX_TRANSFER_TIME) {
      break;
    }
  }

  this->disable();
  this->write_command_(IT8951_TCON_LD_IMG_END);

  return this->transfer_row_ >= area_h;
}

bool EPaperIT8951E::transfer_data() { return this->transfer_row_data_(); }

void EPaperIT8951E::refresh_screen(bool partial) {
  if (this->queued_update_mode_ == UPDATE_MODE_NONE)
    return;

  if (this->is_display_busy_()) {
    this->waiting_for_idle_ = true;
    return;
  }

  this->update_area_(this->pending_x_, this->pending_y_, this->pending_w_, this->pending_h_,
                      this->queued_update_mode_);

  if (this->update_timing_active_) {
    ESP_LOGD(TAG, "Update took %ums (mode=%d area=%ux%u@%u,%u)",
             millis() - this->update_started_at_, static_cast<int>(this->queued_update_mode_),
             this->pending_w_, this->pending_h_, this->pending_x_, this->pending_y_);
    this->update_timing_active_ = false;
  }
}

void EPaperIT8951E::power_on() { this->write_command_(IT8951_TCON_SYS_RUN); }
void EPaperIT8951E::power_off() {}
void EPaperIT8951E::deep_sleep() { this->write_command_(IT8951_TCON_SLEEP); }

void EPaperIT8951E::process_state_() {
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
        return;  // LUT still busy, retry next loop
      }
      this->update_area_(this->pending_x_, this->pending_y_, this->pending_w_, this->pending_h_,
                          this->queued_update_mode_);
      if (this->update_timing_active_) {
        ESP_LOGD(TAG, "Update took %ums (mode=%d area=%ux%u@%u,%u)",
                 millis() - this->update_started_at_, static_cast<int>(this->queued_update_mode_),
                 this->pending_w_, this->pending_h_, this->pending_x_, this->pending_y_);
        this->update_timing_active_ = false;
      }
      this->set_state_(this->sleep_when_done_ ? EPaperState::POWER_OFF : EPaperState::IDLE);
      break;

    case EPaperState::POWER_OFF:
      this->set_state_(EPaperState::DEEP_SLEEP);
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

void EPaperIT8951E::update_fast() {
  if (!this->is_ready() || !this->initialized_)
    return;
  if (this->state_ == EPaperState::IDLE) {
    this->update_started_at_ = millis();
    this->update_timing_active_ = true;
    this->queued_update_mode_ = UPDATE_MODE_DU;
    this->enable_loop();
    this->set_state_(EPaperState::UPDATE);
  } else {
    this->update_pending_ = true;
    this->queued_update_mode_ = UPDATE_MODE_DU;
  }
}

void EPaperIT8951E::update() {
  if (!this->is_ready() || !this->initialized_)
    return;
  if (this->state_ == EPaperState::IDLE) {
    this->update_started_at_ = millis();
    this->update_timing_active_ = true;
    this->queued_update_mode_ = UPDATE_MODE_GC16;
    this->enable_loop();
    this->set_state_(EPaperState::UPDATE);
  } else {
    this->update_pending_ = true;
    this->queued_update_mode_ = UPDATE_MODE_GC16;
  }
}

// --- Color conversion ---

uint8_t EPaperIT8951E::color_to_nibble_(const Color &color) const {
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

void EPaperIT8951E::fill(Color color) {
  if (this->get_clipping().is_set()) {
    Display::fill(color);
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

void EPaperIT8951E::draw_pixel_at(int x, int y, Color color) {
  if (!this->rotate_coordinates_(x, y))
    return;
  this->draw_absolute_pixel_internal(x, y, color);
}

void HOT EPaperIT8951E::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x < 0 || y < 0 || x >= this->get_width_internal() || y >= this->get_height_internal())
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

void EPaperIT8951E::dump_config() {
  LOG_DISPLAY("", "IT8951E E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Dimensions: %dx%d", this->get_width_internal(), this->get_height_internal());
  ESP_LOGCONFIG(TAG, "  Buffer: %u bytes in %u segment(s)",
                static_cast<unsigned>(this->buffer_length_), static_cast<unsigned>(this->buffer_.get_buffer_count()));
  ESP_LOGCONFIG(TAG, "  Image buffer addr: 0x%04X%04X", this->usImgBufAddrH_, this->usImgBufAddrL_);
  ESP_LOGCONFIG(TAG, "  Sleep when done: %s", YESNO(this->sleep_when_done_));
  ESP_LOGCONFIG(TAG, "  Full update every: %u", this->full_update_every_);
  ESP_LOGCONFIG(TAG, "  Reversed colors: %s", YESNO(this->reversed_));
  ESP_LOGCONFIG(TAG, "  Reset duration: %ums", this->reset_duration_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace esphome::epaper_spi
