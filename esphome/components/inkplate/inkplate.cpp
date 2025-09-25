#include "inkplate.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <hal/gpio_hal.h>

namespace esphome {
namespace inkplate {

static const char *const TAG = "inkplate";

void Inkplate::setup() {
  ESP_LOGD(TAG, "Inkplate setup starting");

  if (is_spi_model(this->model_)) {
    ESP_LOGI(TAG, "Setting up Inkplate SPI-based model");

    ESP_LOGD(TAG, "Setting up SPI model pins");
    if (this->epaper_rst_pin_) {
      this->epaper_rst_pin_->setup();
    }
    if (this->epaper_dc_pin_) {
      this->epaper_dc_pin_->setup();
    }
    if (this->epaper_cs_pin_) {
      this->epaper_cs_pin_->setup();
    }
    if (this->epaper_busy_pin_) {
      this->epaper_busy_pin_->setup();
    }

    if (this->epaper_dc_pin_)
      this->epaper_dc_pin_->digital_write(true);
    if (this->epaper_cs_pin_)
      this->epaper_cs_pin_->digital_write(true);
    if (this->epaper_rst_pin_)
      this->epaper_rst_pin_->digital_write(true);

    ESP_LOGD(TAG, "Initializing SPI for SPI-based model");
#ifdef USE_ESP32
    this->spi_class_ = new SPIClass(VSPI);
    if (this->spi_class_ == nullptr) {
      ESP_LOGE(TAG, "Failed to create SPI class");
      return;
    }

    this->spi_settings_ = SPISettings(2000000, MSBFIRST, SPI_MODE0);
    this->spi_class_->begin();
    ESP_LOGD(TAG, "SPI initialized successfully");
#endif

    this->initialize_();
    return;
  }

  // Setup for parallel GPIO models
  ESP_LOGD(TAG, "Setting up Inkplate with parallel interface");

  for (uint32_t i = 0; i < 256; i++) {
    this->pin_lut_[i] = ((i & 0b00000011) << 4) | (((i & 0b00001100) >> 2) << 18) | (((i & 0b00010000) >> 4) << 23) |
                        (((i & 0b11100000) >> 5) << 25);
  }

  this->initialize_();

  if (this->vcom_pin_)
    this->vcom_pin_->setup();
  if (this->powerup_pin_)
    this->powerup_pin_->setup();
  if (this->wakeup_pin_)
    this->wakeup_pin_->setup();
  if (this->gpio0_enable_pin_)
    this->gpio0_enable_pin_->setup();
  if (this->gpio0_enable_pin_)
    this->gpio0_enable_pin_->digital_write(true);

  if (this->cl_pin_)
    this->cl_pin_->setup();
  if (this->le_pin_)
    this->le_pin_->setup();
  if (this->ckv_pin_)
    this->ckv_pin_->setup();
  if (this->gmod_pin_)
    this->gmod_pin_->setup();
  if (this->oe_pin_)
    this->oe_pin_->setup();
  if (this->sph_pin_)
    this->sph_pin_->setup();
  if (this->spv_pin_)
    this->spv_pin_->setup();

  if (this->display_data_0_pin_)
    this->display_data_0_pin_->setup();
  if (this->display_data_1_pin_)
    this->display_data_1_pin_->setup();
  if (this->display_data_2_pin_)
    this->display_data_2_pin_->setup();
  if (this->display_data_3_pin_)
    this->display_data_3_pin_->setup();
  if (this->display_data_4_pin_)
    this->display_data_4_pin_->setup();
  if (this->display_data_5_pin_)
    this->display_data_5_pin_->setup();
  if (this->display_data_6_pin_)
    this->display_data_6_pin_->setup();
  if (this->display_data_7_pin_)
    this->display_data_7_pin_->setup();

  if (this->wakeup_pin_)
    this->wakeup_pin_->digital_write(true);
  delay(1);
#ifdef USE_INKPLATE_I2C
  this->write_bytes(0x09, {
                              0b00011011,  // Power up seq.
                              0b00000000,  // Power up delay (3mS per rail)
                              0b00011011,  // Power down seq.
                              0b00000000,  // Power down delay (6mS per rail)
                          });
#endif
  delay(1);
  this->wakeup_pin_->digital_write(false);
}

Inkplate::~Inkplate() {
#ifdef USE_ESP32
  delete this->spi_class_;  // NOLINT(cppcoreguidelines-owning-memory)
#endif
}

/**
 * Allocate buffers. May be called after setup to re-initialise if e.g. greyscale is changed.
 */
void Inkplate::initialize_() {
  ESP_LOGD(TAG, "Initializing Inkplate buffers");
  RAMAllocator<uint8_t> allocator;
  RAMAllocator<uint32_t> allocator32;

  uint32_t buffer_size = this->get_buffer_length_();
  ESP_LOGD(TAG, "Buffer size: %d bytes", buffer_size);

  if (buffer_size == 0) {
    ESP_LOGW(TAG, "Buffer size is 0, cannot initialize");
    return;
  }

  if (this->partial_buffer_ != nullptr)
    allocator.deallocate(this->partial_buffer_, buffer_size);
  if (this->partial_buffer_2_ != nullptr)
    allocator.deallocate(this->partial_buffer_2_, buffer_size * 2);
  if (this->buffer_ != nullptr)
    allocator.deallocate(this->buffer_, buffer_size);
  if (this->glut_ != nullptr)
    allocator32.deallocate(this->glut_, 256 * 9);
  if (this->glut2_ != nullptr)
    allocator32.deallocate(this->glut2_, 256 * 9);

  this->buffer_ = allocator.allocate(buffer_size);
  if (this->buffer_ == nullptr) {
    ESP_LOGE(TAG, "Could not allocate buffer for display!");
    this->mark_failed();
    return;
  }

  if (is_spi_model(this->model_)) {
    ESP_LOGD(TAG, "Allocating SPI model buffers");
    // SPI models only need the main buffer, no partial buffers or waveform data
    this->partial_buffer_ = allocator.allocate(buffer_size);
    if (this->partial_buffer_ == nullptr) {
      ESP_LOGE(TAG, "Could not allocate partial buffer for SPI display!");
      this->mark_failed();
      return;
    }
    memset(this->partial_buffer_, 0, buffer_size);
    ESP_LOGD(TAG, "SPI model buffers allocated successfully");
  } else if (this->greyscale_) {
    this->glut_ = allocator32.allocate(256 * GLUT_SIZE);
    if (this->glut_ == nullptr) {
      ESP_LOGE(TAG, "Could not allocate glut!");
      this->mark_failed();
      return;
    }
    this->glut2_ = allocator32.allocate(256 * GLUT_SIZE);
    if (this->glut2_ == nullptr) {
      ESP_LOGE(TAG, "Could not allocate glut2!");
      this->mark_failed();
      return;
    }

    for (uint8_t i = 0; i < GLUT_SIZE; i++) {
      for (uint32_t j = 0; j < 256; j++) {
        uint8_t z = (this->waveform_[j & 0x07][i] << 2) | (this->waveform_[(j >> 4) & 0x07][i]);
        this->glut_[i * 256 + j] = ((z & 0b00000011) << 4) | (((z & 0b00001100) >> 2) << 18) |
                                   (((z & 0b00010000) >> 4) << 23) | (((z & 0b11100000) >> 5) << 25);
        z = ((this->waveform_[j & 0x07][i] << 2) | (this->waveform_[(j >> 4) & 0x07][i])) << 4;
        this->glut2_[i * 256 + j] = ((z & 0b00000011) << 4) | (((z & 0b00001100) >> 2) << 18) |
                                    (((z & 0b00010000) >> 4) << 23) | (((z & 0b11100000) >> 5) << 25);
      }
    }

  } else {
    // Parallel GPIO, non-greyscale models need partial buffers
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
    ESP_LOGD(TAG, "Parallel model buffers allocated: %d + %d bytes", buffer_size, buffer_size * 2);
  }

  memset(this->buffer_, 0, buffer_size);
}

float Inkplate::get_setup_priority() const { return setup_priority::PROCESSOR; }

size_t Inkplate::get_buffer_length_() {
  if (this->model_ == INKPLATE_6_COLOR) {
    // COLOR model uses 4-bit per pixel (2 pixels per byte)
    return size_t(this->get_width_internal()) * size_t(this->get_height_internal()) / 2u;
  } else if (this->greyscale_) {
    return size_t(this->get_width_internal()) * size_t(this->get_height_internal()) / 2u;
  } else {
    return size_t(this->get_width_internal()) * size_t(this->get_height_internal()) / 8u;
  }
}

void Inkplate::update() {
  this->do_update_();

  if (this->full_update_every_ > 0 && this->partial_updates_ >= this->full_update_every_) {
    this->block_partial_ = true;
  }

  this->display();
}

void HOT Inkplate::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || y >= this->get_height_internal() || x < 0 || y < 0)
    return;

  if (this->mirror_y_)
    y = this->get_height_internal() - y - 1;

  if (this->mirror_x_)
    x = this->get_width_internal() - x - 1;

  if (this->model_ == INKPLATE_6_COLOR) {
    // COLOR model: 4-bit per pixel, 2 pixels per byte
    int x1 = x / 2;
    int x_sub = x % 2;
    uint32_t pos = (x1 + y * (this->get_width_internal() / 2));
    uint8_t current = this->partial_buffer_[pos];

    uint8_t color_value = this->map_color_to_palette_(color);

    if (x_sub == 0) {
      // Left pixel (upper 4 bits)
      this->partial_buffer_[pos] = (current & 0x0F) | (color_value << 4);
    } else {
      // Right pixel (lower 4 bits)
      this->partial_buffer_[pos] = (current & 0xF0) | color_value;
    }

  } else if (this->greyscale_) {
    int x1 = x / 2;
    int x_sub = x % 2;
    uint32_t pos = (x1 + y * (this->get_width_internal() / 2));
    uint8_t current = this->buffer_[pos];

    // float px = (0.2126 * (color.red / 255.0)) + (0.7152 * (color.green / 255.0)) + (0.0722 * (color.blue / 255.0));
    // px = pow(px, 1.5);
    // uint8_t gs = (uint8_t)(px*7);

    uint8_t gs = ((color.red * 2126 / 10000) + (color.green * 7152 / 10000) + (color.blue * 722 / 10000)) >> 5;
    this->buffer_[pos] = (PIXEL_MASK_GLUT[x_sub] & current) | (x_sub ? gs : gs << 4);

  } else {
    int x1 = x / 8;
    int x_sub = x % 8;
    uint32_t pos = (x1 + y * (this->get_width_internal() / 8));
    uint8_t current = this->partial_buffer_[pos];
    this->partial_buffer_[pos] = (~PIXEL_MASK_LUT[x_sub] & current) | (color.is_on() ? 0 : PIXEL_MASK_LUT[x_sub]);
  }
}

void Inkplate::dump_config() {
  LOG_DISPLAY("", "Inkplate", this);
  ESP_LOGCONFIG(TAG,
                "  Greyscale: %s\n"
                "  Partial Updating: %s\n"
                "  Full Update Every: %d",
                YESNO(this->greyscale_), YESNO(this->partial_updating_), this->full_update_every_);
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

void Inkplate::eink_off_() {
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

#ifdef USE_INKPLATE_I2C
  this->write_byte(0x01, 0x6F);  // Put TPS65186 into standby mode

  delay(100);  // NOLINT

  this->write_byte(0x01, 0x4f);  // Disable 3V3 to the panel
#endif

  if (this->model_ != INKPLATE_6_PLUS)
    this->wakeup_pin_->digital_write(false);

  pins_z_state_();
}

void Inkplate::eink_on_() {
  ESP_LOGV(TAG, "Eink on called");
  if (panel_on_)
    return;
  this->panel_on_ = true;

  this->pins_as_outputs_();
  this->wakeup_pin_->digital_write(true);
  this->vcom_pin_->digital_write(true);
  delay(2);

#ifdef USE_INKPLATE_I2C
  this->write_byte(0x01, 0b00101111);  // Enable all rails

  delay(1);

  this->write_byte(0x01, 0b10101111);  // Switch TPS65186 into active mode
#endif

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

bool Inkplate::read_power_status_() {
#ifdef USE_INKPLATE_I2C
  uint8_t data;
  auto err = this->read_register(0x0F, &data, 1);
  if (err == i2c::ERROR_OK) {
    return data == 0b11111010;
  }
#endif
  return false;
}

void Inkplate::fill(Color color) {
  ESP_LOGVV(TAG, "Fill called with color R:%d G:%d B:%d", color.red, color.green, color.blue);
  uint32_t start_time = millis();

  // If clipping is active, fall back to base implementation
  if (this->get_clipping().is_set()) {
    Display::fill(color);
    ESP_LOGV(TAG, "Fill finished (%ums)", millis() - start_time);
    return;
  }

  if (this->model_ == INKPLATE_6_COLOR) {
    // COLOR model: 4-bit per pixel, 2 pixels per byte
    uint8_t color_value = this->map_color_to_palette_(color);
    ESP_LOGVV(TAG, "Mapped color to palette value: 0x%02X", color_value);
    uint8_t fill_byte = (color_value << 4) | color_value;  // Same color for both pixels in byte
    memset(this->partial_buffer_, fill_byte, this->get_buffer_length_());
  } else if (this->greyscale_) {
    uint8_t fill = ((color.red * 2126 / 10000) + (color.green * 7152 / 10000) + (color.blue * 722 / 10000)) >> 5;
    memset(this->buffer_, (fill << 4) | fill, this->get_buffer_length_());
  } else {
    uint8_t fill = color.is_on() ? 0x00 : 0xFF;
    memset(this->partial_buffer_, fill, this->get_buffer_length_());
  }

  ESP_LOGVV(TAG, "Fill completed in %ums", millis() - start_time);
}

void Inkplate::display() {
  ESP_LOGV(TAG, "Display called");
  uint32_t start_time = millis();

  if (is_spi_model(this->model_)) {
    this->display_color_();
  } else if (this->greyscale_) {
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

void Inkplate::display1b_() {
  ESP_LOGV(TAG, "Display1b called");
  uint32_t start_time = millis();

  memcpy(this->buffer_, this->partial_buffer_, this->get_buffer_length_());

  uint8_t data;
  uint8_t buffer_value;
  const uint8_t *buffer_ptr;
  eink_on_();
  uint8_t rep = 4;
  switch (this->model_) {
    case INKPLATE_10:
      clean_fast_(0, 1);
      clean_fast_(1, 10);
      clean_fast_(2, 1);
      clean_fast_(0, 10);
      clean_fast_(2, 1);
      clean_fast_(1, 10);
      clean_fast_(2, 1);
      clean_fast_(0, 10);
      rep = 5;
      break;
    case INKPLATE_6_PLUS:
      clean_fast_(0, 1);
      clean_fast_(1, 15);
      clean_fast_(2, 1);
      clean_fast_(0, 5);
      clean_fast_(2, 1);
      clean_fast_(1, 15);
      break;
    case INKPLATE_6:
    case INKPLATE_6_V2:
      clean_fast_(0, 1);
      clean_fast_(1, 18);
      clean_fast_(2, 1);
      clean_fast_(0, 18);
      clean_fast_(2, 1);
      clean_fast_(1, 18);
      clean_fast_(2, 1);
      clean_fast_(0, 18);
      clean_fast_(2, 1);
      if (this->model_ == INKPLATE_6_V2)
        rep = 5;
      break;
    case INKPLATE_5:
      clean_fast_(0, 1);
      clean_fast_(1, 14);
      clean_fast_(2, 1);
      clean_fast_(0, 14);
      clean_fast_(2, 1);
      clean_fast_(1, 14);
      clean_fast_(2, 1);
      clean_fast_(0, 14);
      clean_fast_(2, 1);
      rep = 5;
      break;
    case INKPLATE_5_V2:
      clean_fast_(0, 1);
      clean_fast_(1, 11);
      clean_fast_(2, 1);
      clean_fast_(0, 11);
      clean_fast_(2, 1);
      clean_fast_(1, 11);
      clean_fast_(2, 1);
      clean_fast_(0, 11);
      rep = 3;
      break;
  }

  uint32_t clock = (1 << this->cl_pin_->get_pin());
  uint32_t data_mask = this->get_data_pin_mask_();
  ESP_LOGV(TAG, "Display1b start loops (%ums)", millis() - start_time);

  for (uint8_t k = 0; k < rep; k++) {
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
      // New Inkplate6 panel doesn't need last clock
      if (this->model_ != INKPLATE_6_V2) {
        GPIO.out_w1ts = clock;
        GPIO.out_w1tc = data_mask | clock;
      }
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
    // New Inkplate6 panel doesn't need last clock
    if (this->model_ != INKPLATE_6_V2) {
      GPIO.out_w1ts = clock;
      GPIO.out_w1tc = data_mask | clock;
    }
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
      // New Inkplate6 panel doesn't need last clock
      if (this->model_ != INKPLATE_6_V2) {
        GPIO.out_w1ts = clock;
        GPIO.out_w1tc = data_mask | clock;
      }
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

void Inkplate::display3b_() {
  ESP_LOGV(TAG, "Display3b called");
  uint32_t start_time = millis();

  eink_on_();

  switch (this->model_) {
    case INKPLATE_10:
      if (this->custom_waveform_) {
        clean_fast_(1, 1);
        clean_fast_(0, 7);
        clean_fast_(2, 1);
        clean_fast_(1, 12);
        clean_fast_(2, 1);
        clean_fast_(0, 7);
        clean_fast_(2, 1);
        clean_fast_(1, 12);
      } else {
        clean_fast_(1, 1);
        clean_fast_(0, 10);
        clean_fast_(2, 1);
        clean_fast_(1, 10);
        clean_fast_(2, 1);
        clean_fast_(0, 10);
        clean_fast_(2, 1);
        clean_fast_(1, 10);
      }
      break;
    case INKPLATE_6_PLUS:
      clean_fast_(0, 1);
      clean_fast_(1, 15);
      clean_fast_(2, 1);
      clean_fast_(0, 5);
      clean_fast_(2, 1);
      clean_fast_(1, 15);
      break;
    case INKPLATE_6:
    case INKPLATE_6_V2:
      clean_fast_(0, 1);
      clean_fast_(1, 18);
      clean_fast_(2, 1);
      clean_fast_(0, 18);
      clean_fast_(2, 1);
      clean_fast_(1, 18);
      clean_fast_(2, 1);
      clean_fast_(0, 18);
      clean_fast_(2, 1);
      break;
    case INKPLATE_5:
      clean_fast_(0, 1);
      clean_fast_(1, 14);
      clean_fast_(2, 1);
      clean_fast_(0, 14);
      clean_fast_(2, 1);
      clean_fast_(1, 14);
      clean_fast_(2, 1);
      clean_fast_(0, 14);
      clean_fast_(2, 1);
      break;
    case INKPLATE_5_V2:
      clean_fast_(0, 1);
      clean_fast_(1, 11);
      clean_fast_(2, 1);
      clean_fast_(0, 11);
      clean_fast_(2, 1);
      clean_fast_(1, 11);
      clean_fast_(2, 1);
      clean_fast_(0, 11);
      break;
  }

  uint32_t clock = (1 << this->cl_pin_->get_pin());
  uint32_t data_mask = this->get_data_pin_mask_();
  uint32_t pos;
  uint32_t data;
  uint8_t glut_size = 9;
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
      // New Inkplate6 panel doesn't need last clock
      if (this->model_ != INKPLATE_6_V2) {
        GPIO.out_w1ts = clock;
        GPIO.out_w1tc = data_mask | clock;
      }
      vscan_end_();
    }
    delayMicroseconds(230);
  }
  clean_fast_(3, 1);
  vscan_start_();
  eink_off_();
  ESP_LOGV(TAG, "Display3b finished (%ums)", millis() - start_time);
}

void Inkplate::display_color_() {
  ESP_LOGD(TAG, "Starting COLOR display update");
  uint32_t start_time = millis();

  // Wake the panel back up
  if (!this->set_panel_deep_sleep_(false)) {
    ESP_LOGE(TAG, "Failed to wake panel");
    return;
  }

  // Set resolution setting (600x448 for COLOR model)
  uint8_t res_set_data[] = {0x02, 0x58, 0x01, 0xc0};  // From Arduino library
  this->send_command_(0x61);                          // Resolution setting register
  this->send_data_(res_set_data, 4);

  // Push pixel data via SPI
  this->send_command_(0x10);  // Data start transmission register
  // Copy buffer data to display
  memcpy(this->buffer_, this->partial_buffer_, this->get_buffer_length_());

  // Send pixel data via SPI
  ESP_LOGD(TAG, "Sending %d bytes to COLOR display via SPI", this->get_buffer_length_());

#ifdef USE_ESP32
  if (this->spi_class_ == nullptr) {
    ESP_LOGE(TAG, "SPI not initialized for pixel data transfer!");
    return;
  }

  this->epaper_dc_pin_->digital_write(true);   // Data mode
  this->epaper_cs_pin_->digital_write(false);  // Select device

  this->spi_class_->beginTransaction(this->spi_settings_);

  this->spi_class_->writeBytes(this->buffer_, this->get_buffer_length_());

  this->spi_class_->endTransaction();

  this->epaper_cs_pin_->digital_write(true);  // Deselect device
#endif
  ESP_LOGD(TAG, "Sent %d bytes of pixel data via SPI", this->get_buffer_length_());

  // Refresh display sequence
  this->send_command_(0x04);  // Power off register
  // Wait for busy high signal with timeout
  ESP_LOGD(TAG, "Waiting for power off complete");
  ESP_LOGW(TAG, "Initial busy pin state after power off command: %d", this->epaper_busy_pin_->digital_read());

  uint32_t timeout = millis() + 5000;  // 5 second timeout
  uint32_t loop_count = 0;
  while (!this->epaper_busy_pin_->digital_read() && millis() < timeout) {
    delay(1);
    loop_count++;
    if (loop_count % 1000 == 0) {  // Log every second and feed watchdog
      ESP_LOGW(TAG, "Still waiting for busy pin... (count: %d, pin state: %d)", loop_count,
               this->epaper_busy_pin_->digital_read());
      App.feed_wdt();  // Feed the watchdog during wait
    }
  }
  if (millis() >= timeout) {
    ESP_LOGE(TAG, "Timeout waiting for busy pin after power off command (final state: %d)",
             this->epaper_busy_pin_->digital_read());
    ESP_LOGE(TAG, "Display may not be responding to SPI commands. Check wiring and SPI initialization.");
    return;
  }
  ESP_LOGW(TAG, "Busy pin went HIGH after power off");

  ESP_LOGW(TAG, "Sending display refresh command...");
  ESP_LOGW(TAG, "Busy pin state before display refresh command: %d", this->epaper_busy_pin_->digital_read());
  this->send_command_(0x12);  // Display refresh register
  // Wait for busy high signal with timeout
  ESP_LOGW(TAG, "Waiting for busy pin to go HIGH after display refresh...");
  ESP_LOGW(TAG, "Initial busy pin state after display refresh command: %d", this->epaper_busy_pin_->digital_read());
  timeout = millis() + 30000;  // 30 second timeout for refresh
  loop_count = 0;
  while (!this->epaper_busy_pin_->digital_read() && millis() < timeout) {
    delay(1);
    loop_count++;
    if (loop_count % 1000 == 0) {  // Log every second and feed watchdog
      ESP_LOGW(TAG, "Still waiting for display refresh... (count: %d, pin state: %d)", loop_count,
               this->epaper_busy_pin_->digital_read());
      App.feed_wdt();  // Feed the watchdog during long wait
    }
  }
  if (millis() >= timeout) {
    ESP_LOGE(TAG, "Timeout waiting for busy pin after display refresh command (final state: %d)",
             this->epaper_busy_pin_->digital_read());
    return;
  }
  ESP_LOGW(TAG, "Busy pin went HIGH after display refresh");

  this->send_command_(0x02);  // Additional command from Arduino library
  // Wait for busy low signal with timeout
  timeout = millis() + 5000;  // 5 second timeout
  loop_count = 0;
  while (this->epaper_busy_pin_->digital_read() && millis() < timeout) {
    delay(1);
    loop_count++;
    if (loop_count % 1000 == 0) {  // Feed watchdog every second
      App.feed_wdt();
    }
  }
  if (millis() >= timeout) {
    ESP_LOGE(TAG, "Timeout waiting for busy pin to go low after command 0x02");
  }
  delay(200);

  // Put the panel to sleep again
  this->set_panel_deep_sleep_(true);

  ESP_LOGV(TAG, "Display color finished (%ums)", millis() - start_time);
}

void Inkplate::send_command_(uint8_t command) {
  if (!is_spi_model(this->model_))
    return;

  ESP_LOGW(TAG, "Sending SPI command: 0x%02X", command);

#ifdef USE_ESP32
  if (this->spi_class_ == nullptr) {
    ESP_LOGE(TAG, "SPI not initialized!");
    return;
  }

  ESP_LOGW(TAG, "Setting DC=LOW (command mode), CS=LOW");
  this->epaper_dc_pin_->digital_write(false);  // Command mode
  this->epaper_cs_pin_->digital_write(false);  // Select device

  ESP_LOGW(TAG, "Starting SPI transaction for command");
  this->spi_class_->beginTransaction(this->spi_settings_);
  this->spi_class_->transfer(command);
  this->spi_class_->endTransaction();
  ESP_LOGW(TAG, "Command 0x%02X sent via SPI", command);

  this->epaper_cs_pin_->digital_write(true);  // Deselect device
  ESP_LOGV(TAG, "SPI Command: 0x%02X", command);
#endif
}

void Inkplate::send_data_(uint8_t *data, int length) {
  if (!is_spi_model(this->model_))
    return;

  ESP_LOGW(TAG, "Sending SPI data: %d bytes", length);

#ifdef USE_ESP32
  if (this->spi_class_ == nullptr) {
    ESP_LOGE(TAG, "SPI not initialized!");
    return;
  }

  ESP_LOGW(TAG, "Setting DC=HIGH (data mode), CS=LOW");
  this->epaper_dc_pin_->digital_write(true);   // Data mode
  this->epaper_cs_pin_->digital_write(false);  // Select device

  ESP_LOGW(TAG, "Starting SPI transaction for data");
  this->spi_class_->beginTransaction(this->spi_settings_);
  this->spi_class_->writeBytes(data, length);
  this->spi_class_->endTransaction();
  ESP_LOGW(TAG, "Sent %d bytes of data via SPI", length);

  this->epaper_cs_pin_->digital_write(true);  // Deselect device
  ESP_LOGV(TAG, "SPI Data: %d bytes", length);
#endif
}

void Inkplate::send_data_(uint8_t data) {
  if (!is_spi_model(this->model_))
    return;

#ifdef USE_ESP32
  if (this->spi_class_ == nullptr) {
    ESP_LOGE(TAG, "SPI not initialized!");
    return;
  }

  this->epaper_dc_pin_->digital_write(true);   // Data mode
  this->epaper_cs_pin_->digital_write(false);  // Select device

  this->spi_class_->beginTransaction(this->spi_settings_);
  this->spi_class_->transfer(data);
  this->spi_class_->endTransaction();

  this->epaper_cs_pin_->digital_write(true);  // Deselect device
  ESP_LOGV(TAG, "SPI Data: 0x%02X", data);
#endif
}

bool Inkplate::set_panel_deep_sleep_(bool state) {
  if (!state) {
    // Wake the panel - full initialization sequence from Arduino library
    ESP_LOGW(TAG, "Waking panel with full initialization...");

    // Set pin modes
    this->epaper_busy_pin_->pin_mode(gpio::FLAG_INPUT);
    this->epaper_rst_pin_->pin_mode(gpio::FLAG_OUTPUT);
    this->epaper_dc_pin_->pin_mode(gpio::FLAG_OUTPUT);
    this->epaper_cs_pin_->pin_mode(gpio::FLAG_OUTPUT);

    // De-select epaper and set initial states
    this->epaper_dc_pin_->digital_write(true);
    this->epaper_cs_pin_->digital_write(true);

    // Wait to charge capacitors and avoid big in-rush current
    delay(100);

    ESP_LOGW(TAG, "Performing reset sequence...");

    // Reset sequence
    this->epaper_rst_pin_->digital_write(false);
    delay(1);
    this->epaper_rst_pin_->digital_write(true);
    delay(200);

    // Wait for ePaper to be ready by reading busy HIGH signal
    ESP_LOGW(TAG, "Waiting for panel to be ready after reset...");
    uint32_t init_timeout = millis() + 10000;  // 10 second timeout
    while (!this->epaper_busy_pin_->digital_read() && millis() < init_timeout) {
      delay(1);
    }

    if (!this->epaper_busy_pin_->digital_read()) {
      ESP_LOGE(TAG, "Panel not ready after reset - busy pin stuck LOW");
      return false;
    }
    ESP_LOGW(TAG, "Panel ready after reset");

    // Send initialization commands - critical for proper display operation
    ESP_LOGW(TAG, "Sending panel initialization commands...");

    // Panel setting
    uint8_t panel_set_data[] = {0xEF, 0x08};
    this->send_command_(0x00);  // PANEL_SET_REGISTER
    this->send_data_(panel_set_data, 2);

    // Power setting
    uint8_t power_set_data[] = {0x37, 0x00, 0x05, 0x05};
    this->send_command_(0x01);  // POWER_SET_REGISTER
    this->send_data_(power_set_data, 4);

    // Power off sequence setting
    this->send_command_(0x03);  // POWER_OFF_SEQ_SET_REGISTER
    this->send_data_(0x00);

    // Booster soft start
    uint8_t booster_softstart_data[] = {0xC7, 0xC7, 0x1D};
    this->send_command_(0x06);  // BOOSTER_SOFTSTART_REGISTER
    this->send_data_(booster_softstart_data, 3);

    // Temperature sensor enable
    this->send_command_(0x41);  // TEMP_SENSOR_EN_REGISTER
    this->send_data_(0x00);

    // VCOM data interval
    this->send_command_(0x50);  // VCOM_DATA_INTERVAL_REGISTER
    this->send_data_(0x37);

    ESP_LOGW(TAG, "Panel initialization completed, busy pin state: %d", this->epaper_busy_pin_->digital_read());
    return true;
  } else {
    // Put panel to sleep
    ESP_LOGW(TAG, "Putting panel to deep sleep...");
    this->send_command_(0x07);  // Deep sleep command
    this->send_data_(0xA5);     // Deep sleep data
    ESP_LOGW(TAG, "Panel put to sleep");
    return true;
  }
}

uint8_t Inkplate::map_color_to_palette_(Color color) {
  // Map RGB colors to Inkplate COLOR 7-color palette
  // Based on Inkplate6Color.h definitions:
  // INKPLATE_BLACK  0b00000000
  // INKPLATE_WHITE  0b00000001
  // INKPLATE_GREEN  0b00000010
  // INKPLATE_BLUE   0b00000011
  // INKPLATE_RED    0b00000100
  // INKPLATE_YELLOW 0b00000101
  // INKPLATE_ORANGE 0b00000110

  uint8_t r = color.red;
  uint8_t g = color.green;
  uint8_t b = color.blue;

  // White: high R, G, B
  if (r > 200 && g > 200 && b > 200) {
    return 0b00000001;  // INKPLATE_WHITE
  }

  // Black: low R, G, B
  if (r < 50 && g < 50 && b < 50) {
    return 0b00000000;  // INKPLATE_BLACK
  }

  // Red: high R, low G, B
  if (r > 150 && g < 100 && b < 100) {
    return 0b00000100;  // INKPLATE_RED
  }

  // Green: low R, high G, low B
  if (r < 100 && g > 150 && b < 100) {
    return 0b00000010;  // INKPLATE_GREEN
  }

  // Blue: low R, G, high B
  if (r < 100 && g < 100 && b > 150) {
    return 0b00000011;  // INKPLATE_BLUE
  }

  // Orange: high R, medium-high G, low B (check first since it's more specific)
  if (r > 200 && g > 80 && g < 200 && b < 80) {
    return 0b00000110;  // INKPLATE_ORANGE
  }

  // Yellow: high R, G, low B
  if (r > 150 && g > 150 && b < 100) {
    return 0b00000101;  // INKPLATE_YELLOW
  }

  // Default to black for unrecognized colors
  return 0b00000000;  // INKPLATE_BLACK
}

bool Inkplate::partial_update_() {
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

  int rep = (this->model_ == INKPLATE_6_V2) ? 6 : 5;

  eink_on_();
  uint32_t clock = (1 << this->cl_pin_->get_pin());
  uint32_t data_mask = this->get_data_pin_mask_();
  for (int k = 0; k < rep; k++) {
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
      // New Inkplate panel doesn't need last clock
      if (this->model_ != INKPLATE_6_V2) {
        GPIO.out_w1ts = clock;
        GPIO.out_w1tc = data_mask | clock;
      }
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

void Inkplate::vscan_start_() {
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

void Inkplate::hscan_start_(uint32_t d) {
  uint8_t clock = (1 << this->cl_pin_->get_pin());
  this->sph_pin_->digital_write(false);
  GPIO.out_w1ts = d | clock;
  GPIO.out_w1tc = this->get_data_pin_mask_() | clock;
  this->sph_pin_->digital_write(true);
  this->ckv_pin_->digital_write(true);
}

void Inkplate::vscan_end_() {
  this->ckv_pin_->digital_write(false);
  this->le_pin_->digital_write(true);
  this->le_pin_->digital_write(false);
  delayMicroseconds(0);
}

void Inkplate::clean() {
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

void Inkplate::clean_fast_(uint8_t c, uint8_t rep) {
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
      // New Inkplate panel doesn't need last clock
      if (this->model_ != INKPLATE_6_V2) {
        GPIO.out_w1ts = send | clock;
        GPIO.out_w1tc = clock;
      }
      vscan_end_();
    }
    delayMicroseconds(230);
    ESP_LOGV(TAG, "Clean fast rep loop %d finished (%ums)", k, millis() - start_time);
  }
  ESP_LOGV(TAG, "Clean fast finished (%ums)", millis() - start_time);
}

void Inkplate::pins_z_state_() {
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

void Inkplate::pins_as_outputs_() {
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

}  // namespace inkplate
}  // namespace esphome
