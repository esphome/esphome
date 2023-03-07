#include "inkplate.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"

#ifdef USE_ESP32_FRAMEWORK_ARDUINO

#include <esp32-hal-gpio.h>

namespace esphome {
namespace inkplate6 {

static const char *const TAG = "inkplate";

void Inkplate6::setup() {
  for (uint32_t i = 0; i < 256; i++) {
    this->pin_lut_[i] = ((i & 0b00000011) << 4) | (((i & 0b00001100) >> 2) << 18) | (((i & 0b00010000) >> 4) << 23) |
                        (((i & 0b11100000) >> 5) << 25);
  }

  this->initialize_();

  this->vcom_pin_->setup();
  this->powerup_pin_->setup();
  this->wakeup_pin_->setup();
  this->gpio0_enable_pin_->setup();
  this->gpio0_enable_pin_->digital_write(true);

  this->cl_pin_->setup();
  this->le_pin_->setup();
  this->ckv_pin_->setup();
  this->gmod_pin_->setup();
  this->oe_pin_->setup();
  this->sph_pin_->setup();
  this->spv_pin_->setup();

  this->display_data_0_pin_->setup();
  this->display_data_1_pin_->setup();
  this->display_data_2_pin_->setup();
  this->display_data_3_pin_->setup();
  this->display_data_4_pin_->setup();
  this->display_data_5_pin_->setup();
  this->display_data_6_pin_->setup();
  this->display_data_7_pin_->setup();

  this->wakeup_pin_->digital_write(true);
  delay(1);
  this->write_bytes(0x09, {
                              0b00011011,  // Power up seq.
                              0b00000000,  // Power up delay (3mS per rail)
                              0b00011011,  // Power down seq.
                              0b00000000,  // Power down delay (6mS per rail)
                          });
  delay(1);
  this->wakeup_pin_->digital_write(false);
}

void Inkplate6::initialize_() {
  ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
  ExternalRAMAllocator<uint32_t> allocator32(ExternalRAMAllocator<uint32_t>::ALLOW_FAILURE);
  uint32_t buffer_size = this->get_buffer_length_();
  if (buffer_size == 0)
    return;

  if (this->partial_buffer_ != nullptr)
    allocator.deallocate(this->partial_buffer_, buffer_size);
  if (this->partial_buffer_2_ != nullptr)
    allocator.deallocate(this->partial_buffer_2_, buffer_size * 2);
  if (this->buffer_ != nullptr)
    allocator.deallocate(this->buffer_, buffer_size);
  if (this->glut_ != nullptr)
    allocator32.deallocate(this->glut_, 256 * (this->model_ == INKPLATE_6_PLUS ? 9 : 8));
  if (this->glut2_ != nullptr)
    allocator32.deallocate(this->glut2_, 256 * (this->model_ == INKPLATE_6_PLUS ? 9 : 8));

  this->buffer_ = allocator.allocate(buffer_size);
  if (this->buffer_ == nullptr) {
    ESP_LOGE(TAG, "Could not allocate buffer for display!");
    this->mark_failed();
    return;
  }
  if (this->greyscale_) {
    uint8_t glut_size = (this->model_ == INKPLATE_6_PLUS ? 9 : 8);

    this->glut_ = allocator32.allocate(256 * glut_size);
    if (this->glut_ == nullptr) {
      ESP_LOGE(TAG, "Could not allocate glut!");
      this->mark_failed();
      return;
    }
    this->glut2_ = allocator32.allocate(256 * glut_size);
    if (this->glut2_ == nullptr) {
      ESP_LOGE(TAG, "Could not allocate glut2!");
      this->mark_failed();
      return;
    }

    for (int i = 0; i < glut_size; i++) {
      for (uint32_t j = 0; j < 256; j++) {
        uint8_t z = (waveform3Bit[j & 0x07][i] << 2) | (waveform3Bit[(j >> 4) & 0x07][i]);
        this->glut_[i * 256 + j] = ((z & 0b00000011) << 4) | (((z & 0b00001100) >> 2) << 18) |
                                   (((z & 0b00010000) >> 4) << 23) | (((z & 0b11100000) >> 5) << 25);
        z = ((waveform3Bit[j & 0x07][i] << 2) | (waveform3Bit[(j >> 4) & 0x07][i])) << 4;
        this->glut2_[i * 256 + j] = ((z & 0b00000011) << 4) | (((z & 0b00001100) >> 2) << 18) |
                                    (((z & 0b00010000) >> 4) << 23) | (((z & 0b11100000) >> 5) << 25);
      }
    }

  } else {
    this->partial_buffer_ = allocator.allocate(buffer_size);
    if (this->partial_buffer_ == nullptr) {
      ESP_LOGE(TAG, "Could not allocate partial buffer for display!");
      this->mark_failed();
      return;
    }
    this->partial_buffer_2_ = allocator.allocate(buffer_size * 2);
    if (this->partial_buffer_2_ == nullptr) {
      ESP_LOGE(TAG, "Could not allocate partial buffer 2 for display!");
      this->mark_failed();
      return;
    }

    memset(this->partial_buffer_, 0, buffer_size);
    memset(this->partial_buffer_2_, 0, buffer_size * 2);
  }

  memset(this->buffer_, 0, buffer_size);
}

float Inkplate6::get_setup_priority() const { return setup_priority::PROCESSOR; }

size_t Inkplate6::get_buffer_length_() {
  if (this->greyscale_) {
    return size_t(this->get_width_internal()) * size_t(this->get_height_internal()) / 2u;
  } else {
    return size_t(this->get_width_internal()) * size_t(this->get_height_internal()) / 8u;
  }
}

void Inkplate6::update() {
  this->do_update_();

  if (this->full_update_every_ > 0 && this->partial_updates_ >= this->full_update_every_) {
    this->block_partial_ = true;
  }

  this->display();
}

void HOT Inkplate6::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || y >= this->get_height_internal() || x < 0 || y < 0)
    return;

  if (this->greyscale_) {
    int x1 = x / 2;
    int x_sub = x % 2;
    uint32_t pos = (x1 + y * (this->get_width_internal() / 2));
    uint8_t current = this->buffer_[pos];

    // float px = (0.2126 * (color.red / 255.0)) + (0.7152 * (color.green / 255.0)) + (0.0722 * (color.blue / 255.0));
    // px = pow(px, 1.5);
    // uint8_t gs = (uint8_t)(px*7);

    uint8_t gs = ((color.red * 2126 / 10000) + (color.green * 7152 / 10000) + (color.blue * 722 / 10000)) >> 5;
    this->buffer_[pos] = (pixelMaskGLUT[x_sub] & current) | (x_sub ? gs : gs << 4);

  } else {
    int x1 = x / 8;
    int x_sub = x % 8;
    uint32_t pos = (x1 + y * (this->get_width_internal() / 8));
    uint8_t current = this->partial_buffer_[pos];
    this->partial_buffer_[pos] = (~pixelMaskLUT[x_sub] & current) | (color.is_on() ? 0 : pixelMaskLUT[x_sub]);
  }
}

void Inkplate6::dump_config() {
  LOG_DISPLAY("", "Inkplate", this);
  ESP_LOGCONFIG(TAG, "  Greyscale: %s", YESNO(this->greyscale_));
  ESP_LOGCONFIG(TAG, "  Partial Updating: %s", YESNO(this->partial_updating_));
  ESP_LOGCONFIG(TAG, "  Full Update Every: %d", this->full_update_every_);
  // Log pins
  LOG_PIN("  CKV Pin: ", this->ckv_pin_);
  LOG_PIN("  CL Pin: ", this->cl_pin_);
  LOG_PIN("  GPIO0 Enable Pin: ", this->gpio0_enable_pin_);
  LOG_PIN("  GMOD Pin: ", this->gmod_pin_);
  LOG_PIN("  LE Pin: ", this->le_pin_);
  LOG_PIN("  OE Pin: ", this->oe_pin_);
  LOG_PIN("  POWERUP Pin: ", this->powerup_pin_);
  LOG_PIN("  SPH Pin: ", this->sph_pin_);
  LOG_PIN("  SPV Pin: ", this->spv_pin_);
  LOG_PIN("  VCOM Pin: ", this->vcom_pin_);
  LOG_PIN("  WAKEUP Pin: ", this->wakeup_pin_);

  LOG_PIN("  Data 0 Pin: ", this->display_data_0_pin_);
  LOG_PIN("  Data 1 Pin: ", this->display_data_1_pin_);
  LOG_PIN("  Data 2 Pin: ", this->display_data_2_pin_);
  LOG_PIN("  Data 3 Pin: ", this->display_data_3_pin_);
  LOG_PIN("  Data 4 Pin: ", this->display_data_4_pin_);
  LOG_PIN("  Data 5 Pin: ", this->display_data_5_pin_);
  LOG_PIN("  Data 6 Pin: ", this->display_data_6_pin_);
  LOG_PIN("  Data 7 Pin: ", this->display_data_7_pin_);

  LOG_UPDATE_INTERVAL(this);
}

void Inkplate6::eink_off_() {
  ESP_LOGV(TAG, "Eink off called");
  if (!panel_on_)
    return;
  panel_on_ = false;

  this->oe_pin_->digital_write(false);
  this->gmod_pin_->digital_write(false);

  GPIO.out &= ~(this->get_data_pin_mask_() | (1 << this->cl_pin_->get_pin()) | (1 << this->le_pin_->get_pin()));
  this->ckv_pin_->digital_write(false);
  this->sph_pin_->digital_write(false);
  this->spv_pin_->digital_write(false);

  this->vcom_pin_->digital_write(false);

  this->write_byte(0x01, 0x6F);  // Put TPS65186 into standby mode

  delay(100);  // NOLINT

  this->write_byte(0x01, 0x4f);  // Disable 3V3 to the panel

  if (this->model_ != INKPLATE_6_PLUS)
    this->wakeup_pin_->digital_write(false);

  pins_z_state_();
}

void Inkplate6::eink_on_() {
  ESP_LOGV(TAG, "Eink on called");
  if (panel_on_)
    return;
  this->panel_on_ = true;

  this->pins_as_outputs_();
  this->wakeup_pin_->digital_write(true);
  this->vcom_pin_->digital_write(true);
  delay(2);

  this->write_byte(0x01, 0b00101111);  // Enable all rails

  delay(1);

  this->write_byte(0x01, 0b10101111);  // Switch TPS65186 into active mode

  this->le_pin_->digital_write(false);
  this->oe_pin_->digital_write(false);
  this->cl_pin_->digital_write(false);
  this->sph_pin_->digital_write(true);
  this->gmod_pin_->digital_write(true);
  this->spv_pin_->digital_write(true);
  this->ckv_pin_->digital_write(false);
  this->oe_pin_->digital_write(false);

  uint32_t timer = millis();
  do {
    delay(1);
  } while (!this->read_power_status_() && ((millis() - timer) < 250));
  if ((millis() - timer) >= 250) {
    ESP_LOGW(TAG, "Power supply not detected");
    this->wakeup_pin_->digital_write(false);
    this->vcom_pin_->digital_write(false);
    this->powerup_pin_->digital_write(false);
    this->panel_on_ = false;
    return;
  }

  this->oe_pin_->digital_write(true);
}

bool Inkplate6::read_power_status_() {
  uint8_t data;
  auto err = this->read_register(0x0F, &data, 1);
  if (err == i2c::ERROR_OK) {
    return data == 0b11111010;
  }
  return false;
}

void Inkplate6::fill(Color color) {
  ESP_LOGV(TAG, "Fill called");
  uint32_t start_time = millis();

  if (this->greyscale_) {
    uint8_t fill = ((color.red * 2126 / 10000) + (color.green * 7152 / 10000) + (color.blue * 722 / 10000)) >> 5;
    memset(this->buffer_, (fill << 4) | fill, this->get_buffer_length_());
  } else {
    uint8_t fill = color.is_on() ? 0x00 : 0xFF;
    memset(this->partial_buffer_, fill, this->get_buffer_length_());
  }

  ESP_LOGV(TAG, "Fill finished (%ums)", millis() - start_time);
}

void Inkplate6::display() {
  ESP_LOGV(TAG, "Display called");
  uint32_t start_time = millis();

  if (this->greyscale_) {
    this->display3b_();
  } else {
    if (this->partial_updating_ && this->partial_update_()) {
      ESP_LOGV(TAG, "Display finished (partial) (%ums)", millis() - start_time);
      return;
    }
    this->display1b_();
  }
  ESP_LOGV(TAG, "Display finished (full) (%ums)", millis() - start_time);
}

void Inkplate6::display1b_() {
  ESP_LOGV(TAG, "Display1b called");
  uint32_t start_time = millis();

  memcpy(this->buffer_, this->partial_buffer_, this->get_buffer_length_());

  uint8_t data;
  uint8_t buffer_value;
  const uint8_t *buffer_ptr;
  eink_on_();
  if (this->model_ == INKPLATE_6_PLUS) {
    clean_fast_(0, 1);
    clean_fast_(1, 15);
    clean_fast_(2, 1);
    clean_fast_(0, 5);
    clean_fast_(2, 1);
    clean_fast_(1, 15);
  } else {
    clean_fast_(0, 1);
    clean_fast_(1, 21);
    clean_fast_(2, 1);
    clean_fast_(0, 12);
    clean_fast_(2, 1);
    clean_fast_(1, 21);
    clean_fast_(2, 1);
    clean_fast_(0, 12);
  }

  uint32_t clock = (1 << this->cl_pin_->get_pin());
  uint32_t data_mask = this->get_data_pin_mask_();
  ESP_LOGV(TAG, "Display1b start loops (%ums)", millis() - start_time);

  for (int k = 0; k < 4; k++) {
    buffer_ptr = &this->buffer_[this->get_buffer_length_() - 1];
    vscan_start_();
    for (int i = 0, im = this->get_height_internal(); i < im; i++) {
      buffer_value = *(buffer_ptr--);
      data = this->model_ == INKPLATE_6_PLUS ? LUTW[(~buffer_value >> 4) & 0x0F] : LUTB[(buffer_value >> 4) & 0x0F];
      hscan_start_(this->pin_lut_[data]);
      data = this->model_ == INKPLATE_6_PLUS ? LUTW[(~buffer_value) & 0x0F] : LUTB[buffer_value & 0x0F];
      GPIO.out_w1ts = this->pin_lut_[data] | clock;
      GPIO.out_w1tc = data_mask | clock;

      for (int j = 0, jm = (this->get_width_internal() / 8) - 1; j < jm; j++) {
        buffer_value = *(buffer_ptr--);
        data = this->model_ == INKPLATE_6_PLUS ? LUTW[(~buffer_value >> 4) & 0x0F] : LUTB[(buffer_value >> 4) & 0x0F];
        GPIO.out_w1ts = this->pin_lut_[data] | clock;
        GPIO.out_w1tc = data_mask | clock;
        data = this->model_ == INKPLATE_6_PLUS ? LUTW[(~buffer_value) & 0x0F] : LUTB[buffer_value & 0x0F];
        GPIO.out_w1ts = this->pin_lut_[data] | clock;
        GPIO.out_w1tc = data_mask | clock;
      }
      GPIO.out_w1ts = clock;
      GPIO.out_w1tc = data_mask | clock;
      vscan_end_();
    }
    delayMicroseconds(230);
  }
  ESP_LOGV(TAG, "Display1b first loop x %d (%ums)", 4, millis() - start_time);

  buffer_ptr = &this->buffer_[this->get_buffer_length_() - 1];
  vscan_start_();
  for (int i = 0, im = this->get_height_internal(); i < im; i++) {
    buffer_value = *(buffer_ptr--);
    data = this->model_ == INKPLATE_6_PLUS ? LUTB[(buffer_value >> 4) & 0x0F] : LUT2[(buffer_value >> 4) & 0x0F];
    hscan_start_(this->pin_lut_[data] | clock);
    data = this->model_ == INKPLATE_6_PLUS ? LUTB[buffer_value & 0x0F] : LUT2[buffer_value & 0x0F];
    GPIO.out_w1ts = this->pin_lut_[data] | clock;
    GPIO.out_w1tc = data_mask | clock;

    for (int j = 0, jm = (this->get_width_internal() / 8) - 1; j < jm; j++) {
      buffer_value = *(buffer_ptr--);
      data = this->model_ == INKPLATE_6_PLUS ? LUTB[(buffer_value >> 4) & 0x0F] : LUT2[(buffer_value >> 4) & 0x0F];
      GPIO.out_w1ts = this->pin_lut_[data] | clock;
      GPIO.out_w1tc = data_mask | clock;
      data = this->model_ == INKPLATE_6_PLUS ? LUTB[buffer_value & 0x0F] : LUT2[buffer_value & 0x0F];
      GPIO.out_w1ts = this->pin_lut_[data] | clock;
      GPIO.out_w1tc = data_mask | clock;
    }
    GPIO.out_w1ts = clock;
    GPIO.out_w1tc = data_mask | clock;
    vscan_end_();
  }
  delayMicroseconds(230);
  ESP_LOGV(TAG, "Display1b second loop (%ums)", millis() - start_time);

  if (this->model_ == INKPLATE_6_PLUS) {
    clean_fast_(2, 2);
    clean_fast_(3, 1);
  } else {
    uint32_t send = this->pin_lut_[0];
    vscan_start_();
    for (int i = 0, im = this->get_height_internal(); i < im; i++) {
      hscan_start_(send);
      GPIO.out_w1ts = send | clock;
      GPIO.out_w1tc = data_mask | clock;
      for (int j = 0, jm = (this->get_width_internal() / 8) - 1; j < jm; j++) {
        GPIO.out_w1ts = send | clock;
        GPIO.out_w1tc = data_mask | clock;
        GPIO.out_w1ts = send | clock;
        GPIO.out_w1tc = data_mask | clock;
      }
      GPIO.out_w1ts = send | clock;
      GPIO.out_w1tc = data_mask | clock;
      vscan_end_();
    }
    delayMicroseconds(230);
    ESP_LOGV(TAG, "Display1b third loop (%ums)", millis() - start_time);
  }
  vscan_start_();
  eink_off_();
  this->block_partial_ = false;
  this->partial_updates_ = 0;
  ESP_LOGV(TAG, "Display1b finished (%ums)", millis() - start_time);
}

void Inkplate6::display3b_() {
  ESP_LOGV(TAG, "Display3b called");
  uint32_t start_time = millis();

  eink_on_();
  if (this->model_ == INKPLATE_6_PLUS) {
    clean_fast_(0, 1);
    clean_fast_(1, 15);
    clean_fast_(2, 1);
    clean_fast_(0, 5);
    clean_fast_(2, 1);
    clean_fast_(1, 15);
  } else {
    clean_fast_(0, 1);
    clean_fast_(1, 21);
    clean_fast_(2, 1);
    clean_fast_(0, 12);
    clean_fast_(2, 1);
    clean_fast_(1, 21);
    clean_fast_(2, 1);
    clean_fast_(0, 12);
  }

  uint32_t clock = (1 << this->cl_pin_->get_pin());
  uint32_t data_mask = this->get_data_pin_mask_();
  uint32_t pos;
  uint32_t data;
  uint8_t glut_size = this->model_ == INKPLATE_6_PLUS ? 9 : 8;
  for (int k = 0; k < glut_size; k++) {
    pos = this->get_buffer_length_();
    vscan_start_();
    for (int i = 0; i < this->get_height_internal(); i++) {
      data = this->glut2_[k * 256 + this->buffer_[--pos]];
      data |= this->glut_[k * 256 + this->buffer_[--pos]];
      hscan_start_(data);
      data = this->glut2_[k * 256 + this->buffer_[--pos]];
      data |= this->glut_[k * 256 + this->buffer_[--pos]];
      GPIO.out_w1ts = data | clock;
      GPIO.out_w1tc = data_mask | clock;

      for (int j = 0; j < (this->get_width_internal() / 8) - 1; j++) {
        data = this->glut2_[k * 256 + this->buffer_[--pos]];
        data |= this->glut_[k * 256 + this->buffer_[--pos]];
        GPIO.out_w1ts = data | clock;
        GPIO.out_w1tc = data_mask | clock;
        data = this->glut2_[k * 256 + this->buffer_[--pos]];
        data |= this->glut_[k * 256 + this->buffer_[--pos]];
        GPIO.out_w1ts = data | clock;
        GPIO.out_w1tc = data_mask | clock;
      }
      GPIO.out_w1ts = clock;
      GPIO.out_w1tc = data_mask | clock;
      vscan_end_();
    }
    delayMicroseconds(230);
  }
  clean_fast_(3, 1);
  vscan_start_();
  eink_off_();
  ESP_LOGV(TAG, "Display3b finished (%ums)", millis() - start_time);
}

bool Inkplate6::partial_update_() {
  ESP_LOGV(TAG, "Partial update called");
  uint32_t start_time = millis();
  if (this->greyscale_)
    return false;
  if (this->block_partial_)
    return false;

  this->partial_updates_++;

  uint32_t pos = this->get_buffer_length_() - 1;
  uint8_t data;
  uint8_t diffw, diffb;
  uint32_t n = (this->get_buffer_length_() * 2) - 1;

  for (int i = 0, im = this->get_height_internal(); i < im; i++) {
    for (int j = 0, jm = (this->get_width_internal() / 8); j < jm; j++) {
      diffw = this->buffer_[pos] & ~(this->partial_buffer_[pos]);
      diffb = ~(this->buffer_[pos]) & this->partial_buffer_[pos];
      pos--;
      this->partial_buffer_2_[n--] = LUTW[diffw >> 4] & LUTB[diffb >> 4];
      this->partial_buffer_2_[n--] = LUTW[diffw & 0x0F] & LUTB[diffb & 0x0F];
    }
  }
  ESP_LOGV(TAG, "Partial update buffer built after (%ums)", millis() - start_time);

  eink_on_();
  uint32_t clock = (1 << this->cl_pin_->get_pin());
  uint32_t data_mask = this->get_data_pin_mask_();
  for (int k = 0; k < 5; k++) {
    vscan_start_();
    const uint8_t *data_ptr = &this->partial_buffer_2_[(this->get_buffer_length_() * 2) - 1];
    for (int i = 0; i < this->get_height_internal(); i++) {
      data = *(data_ptr--);
      hscan_start_(this->pin_lut_[data]);
      for (int j = 0, jm = (this->get_width_internal() / 4) - 1; j < jm; j++) {
        data = *(data_ptr--);
        GPIO.out_w1ts = this->pin_lut_[data] | clock;
        GPIO.out_w1tc = data_mask | clock;
      }
      GPIO.out_w1ts = clock;
      GPIO.out_w1tc = data_mask | clock;
      vscan_end_();
    }
    delayMicroseconds(230);
    ESP_LOGV(TAG, "Partial update loop k=%d (%ums)", k, millis() - start_time);
  }
  clean_fast_(2, 2);
  clean_fast_(3, 1);
  vscan_start_();
  eink_off_();

  memcpy(this->buffer_, this->partial_buffer_, this->get_buffer_length_());
  ESP_LOGV(TAG, "Partial update finished (%ums)", millis() - start_time);
  return true;
}

void Inkplate6::vscan_start_() {
  this->ckv_pin_->digital_write(true);
  delayMicroseconds(7);
  this->spv_pin_->digital_write(false);
  delayMicroseconds(10);
  this->ckv_pin_->digital_write(false);
  delayMicroseconds(0);
  this->ckv_pin_->digital_write(true);
  delayMicroseconds(8);
  this->spv_pin_->digital_write(true);
  delayMicroseconds(10);
  this->ckv_pin_->digital_write(false);
  delayMicroseconds(0);
  this->ckv_pin_->digital_write(true);
  delayMicroseconds(18);
  this->ckv_pin_->digital_write(false);
  delayMicroseconds(0);
  this->ckv_pin_->digital_write(true);
  delayMicroseconds(18);
  this->ckv_pin_->digital_write(false);
  delayMicroseconds(0);
  this->ckv_pin_->digital_write(true);
}

void Inkplate6::hscan_start_(uint32_t d) {
  uint8_t clock = (1 << this->cl_pin_->get_pin());
  this->sph_pin_->digital_write(false);
  GPIO.out_w1ts = d | clock;
  GPIO.out_w1tc = this->get_data_pin_mask_() | clock;
  this->sph_pin_->digital_write(true);
  this->ckv_pin_->digital_write(true);
}

void Inkplate6::vscan_end_() {
  this->ckv_pin_->digital_write(false);
  this->le_pin_->digital_write(true);
  this->le_pin_->digital_write(false);
  delayMicroseconds(0);
}

void Inkplate6::clean() {
  ESP_LOGV(TAG, "Clean called");
  uint32_t start_time = millis();

  eink_on_();
  clean_fast_(0, 1);   // White
  clean_fast_(0, 8);   // White to White
  clean_fast_(0, 1);   // White to Black
  clean_fast_(0, 8);   // Black to Black
  clean_fast_(2, 1);   // Black to White
  clean_fast_(1, 10);  // White to White
  ESP_LOGV(TAG, "Clean finished (%ums)", millis() - start_time);
}

void Inkplate6::clean_fast_(uint8_t c, uint8_t rep) {
  ESP_LOGV(TAG, "Clean fast called with: (%d, %d)", c, rep);
  uint32_t start_time = millis();

  eink_on_();
  uint8_t data = 0;
  if (c == 0) {  // White
    data = 0b10101010;
  } else if (c == 1) {  // Black
    data = 0b01010101;
  } else if (c == 2) {  // Discharge
    data = 0b00000000;
  } else if (c == 3) {  // Skip
    data = 0b11111111;
  }

  uint32_t send = ((data & 0b00000011) << 4) | (((data & 0b00001100) >> 2) << 18) | (((data & 0b00010000) >> 4) << 23) |
                  (((data & 0b11100000) >> 5) << 25);
  uint32_t clock = (1 << this->cl_pin_->get_pin());

  for (int k = 0; k < rep; k++) {
    vscan_start_();
    for (int i = 0; i < this->get_height_internal(); i++) {
      hscan_start_(send);
      GPIO.out_w1ts = send | clock;
      GPIO.out_w1tc = clock;
      for (int j = 0; j < (this->get_width_internal() / 8) - 1; j++) {
        GPIO.out_w1ts = clock;
        GPIO.out_w1tc = clock;
        GPIO.out_w1ts = clock;
        GPIO.out_w1tc = clock;
      }
      GPIO.out_w1ts = send | clock;
      GPIO.out_w1tc = clock;
      vscan_end_();
    }
    delayMicroseconds(230);
    ESP_LOGV(TAG, "Clean fast rep loop %d finished (%ums)", k, millis() - start_time);
  }
  ESP_LOGV(TAG, "Clean fast finished (%ums)", millis() - start_time);
}

void Inkplate6::pins_z_state_() {
  this->cl_pin_->pin_mode(gpio::FLAG_INPUT);
  this->le_pin_->pin_mode(gpio::FLAG_INPUT);
  this->ckv_pin_->pin_mode(gpio::FLAG_INPUT);
  this->sph_pin_->pin_mode(gpio::FLAG_INPUT);

  this->oe_pin_->pin_mode(gpio::FLAG_INPUT);
  this->gmod_pin_->pin_mode(gpio::FLAG_INPUT);
  this->spv_pin_->pin_mode(gpio::FLAG_INPUT);

  this->display_data_0_pin_->pin_mode(gpio::FLAG_INPUT);
  this->display_data_1_pin_->pin_mode(gpio::FLAG_INPUT);
  this->display_data_2_pin_->pin_mode(gpio::FLAG_INPUT);
  this->display_data_3_pin_->pin_mode(gpio::FLAG_INPUT);
  this->display_data_4_pin_->pin_mode(gpio::FLAG_INPUT);
  this->display_data_5_pin_->pin_mode(gpio::FLAG_INPUT);
  this->display_data_6_pin_->pin_mode(gpio::FLAG_INPUT);
  this->display_data_7_pin_->pin_mode(gpio::FLAG_INPUT);
}

void Inkplate6::pins_as_outputs_() {
  this->cl_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->le_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->ckv_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->sph_pin_->pin_mode(gpio::FLAG_OUTPUT);

  this->oe_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->gmod_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->spv_pin_->pin_mode(gpio::FLAG_OUTPUT);

  this->display_data_0_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->display_data_1_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->display_data_2_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->display_data_3_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->display_data_4_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->display_data_5_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->display_data_6_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->display_data_7_pin_->pin_mode(gpio::FLAG_OUTPUT);
}

}  // namespace inkplate6
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ARDUINO
