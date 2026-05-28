#include "epaper_spi_t133a01.h"

#include <algorithm>

#include "driver/gpio.h"
#include "esphome/core/log.h"

namespace esphome::epaper_spi {

static constexpr const char *const TAG = "epaper_spi.t133a01";

// Color indices used in the 4bpp buffer (sprite-side)
// These MUST match the Arduino GFX TFT_eSPI.h color definitions and
// the remap_color()/COLOR_GET mapping:
//   0x0F=BLACK, 0x00=WHITE, 0x02=GREEN, 0x06=RED, 0x0B=YELLOW, 0x0D=BLUE
static constexpr uint8_t T133A01_BLACK = 0x0F;
static constexpr uint8_t T133A01_WHITE = 0x00;
static constexpr uint8_t T133A01_GREEN = 0x02;
static constexpr uint8_t T133A01_RED = 0x06;
static constexpr uint8_t T133A01_YELLOW = 0x0B;
static constexpr uint8_t T133A01_BLUE = 0x0D;

// T133A01 register addresses
static constexpr uint8_t R00_PSR = 0x00;
static constexpr uint8_t R01_PWR = 0x01;
static constexpr uint8_t R02_POF = 0x02;
static constexpr uint8_t R04_PON = 0x04;
static constexpr uint8_t R05_BTST_N = 0x05;
static constexpr uint8_t R06_BTST_P = 0x06;
static constexpr uint8_t R10_DTM = 0x10;
static constexpr uint8_t R12_DRF = 0x12;
static constexpr uint8_t R50_CDI = 0x50;
static constexpr uint8_t R61_TRES = 0x61;
static constexpr uint8_t RA5_DCDC = 0xA5;
static constexpr uint8_t RE0_CCSET = 0xE0;
static constexpr uint8_t RE3_PWS = 0xE3;

/**
 * COLOR_GET remap table from T133A01_Defines.h.
 * Translates 4bpp sprite color index to the hardware pixel encoding.
 *   Sprite: 0x0F=BLACK 0x00=WHITE 0x02=GREEN 0x06=RED 0x0B=YELLOW 0x0D=BLUE
 *   HW:     0x00=BLACK 0x01=WHITE 0x06=GREEN 0x03=RED  0x02=YELLOW 0x05=BLUE
 */
uint8_t EPaperT133A01::remap_color(uint8_t index) {
  switch (index & 0x0F) {
    case 0x0F:
      return 0x00;  // Black
    case 0x00:
      return 0x01;  // White
    case 0x02:
      return 0x06;  // Green
    case 0x06:
      return 0x03;  // Red
    case 0x0B:
      return 0x02;  // Yellow
    case 0x0D:
      return 0x05;  // Blue
    default:
      return 0x01;  // White fallback
  }
}

/**
 * Map an ESPHome Color to a 4-bit sprite color index.
 * Index values match the Arduino GFX TFT_eSPI color definitions:
 *   0x00=WHITE, 0x02=GREEN, 0x06=RED, 0x0B=YELLOW, 0x0D=BLUE, 0x0F=BLACK
 */
uint8_t EPaperT133A01::color_to_index(Color color) {
  unsigned char max_rgb = std::max({color.r, color.g, color.b});
  unsigned char min_rgb = std::min({color.r, color.g, color.b});

  // Check for grayscale
  if ((max_rgb - min_rgb) < 50) {
    if ((static_cast<int>(color.r) + color.g + color.b) > 382) {
      return T133A01_WHITE;
    }
    return T133A01_BLACK;
  }

  bool r_on = (color.r > 128);
  bool g_on = (color.g > 128);
  bool b_on = (color.b > 128);

  if (r_on && g_on && !b_on)
    return T133A01_YELLOW;
  if (r_on && !g_on && !b_on)
    return T133A01_RED;
  if (!r_on && g_on && !b_on)
    return T133A01_GREEN;
  if (!r_on && !g_on && b_on)
    return T133A01_BLUE;
  // Handle mixed colors: map to nearest primary
  if (!r_on && g_on && b_on)
    return T133A01_GREEN;  // Cyan -> Green
  if (r_on && !g_on)
    return T133A01_RED;  // Magenta -> Red
  if (r_on)
    return T133A01_WHITE;
  return T133A01_BLACK;
}

void EPaperT133A01::setup_pins_() const {
  // Set up base pins
  this->dc_pin_->setup();
  this->dc_pin_->digital_write(false);

  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
  }

  if (this->busy_pin_ != nullptr) {
    this->busy_pin_->setup();
  }

  // Set up CS1 pin
  if (this->cs1_pin_ != nullptr) {
    this->cs1_pin_->setup();
    this->cs1_pin_->digital_write(true);
  }

  // Set up ENABLE pin
  if (this->enable_pin_ != nullptr) {
    this->enable_pin_->setup();
    this->enable_pin_->digital_write(true);
  }

  // Reconfigure GPIO10 as pure GPIO output.
  // SPIDelegate already set it up via cs_pin_, but we need to ensure
  // gpio_set_level() (or direct register writes) can override it during
  // the CS1 data phase to deselect the CS controller.
  gpio_config_t io_conf = {};
  io_conf.pin_bit_mask = (1ULL << GPIO_NUM_10);
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&io_conf);
  gpio_set_level(GPIO_NUM_10, 1);
}

bool EPaperT133A01::reset() {
  if (this->enable_pin_ != nullptr) {
    this->enable_pin_->digital_write(true);
  }
  if (this->reset_pin_ != nullptr) {
    if (this->state_ == EPaperState::RESET) {
      this->reset_pin_->digital_write(false);
      return false;
    }
    this->reset_pin_->digital_write(true);
  }
  return true;
}

/**
 * Initialise the T133A01 display.
 *
 * The init sequence uses a mix of CS and CS1 commands as per the Arduino driver.
 * The base class init_sequence is NOT used for T133A01 because the dual-CS
 * protocol requires per-command routing.
 */
bool EPaperT133A01::initialise(bool partial) {
  // SPIDelegate manages CS (GPIO10) automatically via enable()/disable().
  // cs1_pin_ manages CS1 (GPIO2) via digital_write in cmd_data_cs1_().
  //
  // Init sequence mirrors the Arduino GFX library's EPD_INIT() macro
  // (T133A01_Defines.h). Commands prefixed with CS1 (via cmd_data_cs1_)
  // are routed to both CS and CS1 simultaneously, matching Arduino behavior.

  // 0x74 - panel config (CS only, CS1 stays deselected)
  this->cmd_data(0x74, {0x00, 0x0C, 0x0C, 0xD9, 0xDD, 0xDD, 0x15, 0x15, 0x55});
  delay(10);

  // 0xF0 - panel config (CS + CS1)
  this->cmd_data_cs1_(0xF0, {0x49, 0x55, 0x13, 0x5D, 0x05, 0x10});
  delay(10);

  // PSR - Panel Setting Register (CS + CS1)
  this->cmd_data_cs1_(0x00, {0xDF, 0x69});
  delay(10);

  // DCDC (via CS)
  this->cmd_data(RA5_DCDC, {0x44, 0x54, 0x00});
  delay(10);

  // CDI (via CS1)
  this->cmd_data_cs1_(R50_CDI, {0x37});
  delay(10);

  // 0x60 (via CS1)
  this->cmd_data_cs1_(0x60, {0x03, 0x03});
  delay(10);

  // 0x86 (via CS1)
  this->cmd_data_cs1_(0x86, {0x10});
  delay(10);

  // PWS - Phase Width Setting (via CS1)
  this->cmd_data_cs1_(RE3_PWS, {0x22});
  delay(10);

  // TRES - Resolution Setting (CS + CS1).
  // Fixed value {0x04, 0xB0, 0x03, 0x20} matching Arduino TRES_V.
  // With width=1200, height=1600: first word = width = 1200, second word = height/2 = 800.
  this->cmd_data_cs1_(R61_TRES, {
      (uint8_t) (this->width_ >> 8), (uint8_t) (this->width_ & 0xFF),
      (uint8_t) ((this->height_ / 2) >> 8), (uint8_t) ((this->height_ / 2) & 0xFF)});
  delay(10);

  // PWR - Power Setting (via CS)
  this->cmd_data(R01_PWR, {0x0F, 0x00, 0x28, 0x2C, 0x28, 0x38});
  delay(10);

  // 0xB6 (via CS)
  this->cmd_data(0xB6, {0x07});
  delay(10);

  // BTST_P (via CS)
  this->cmd_data(R06_BTST_P, {0xE0, 0x20});
  delay(10);

  // 0xB7 (via CS)
  this->cmd_data(0xB7, {0x01});
  delay(10);

  // BTST_N (via CS)
  this->cmd_data(R05_BTST_N, {0xE0, 0x20});
  delay(10);

  // 0xB0 (via CS)
  this->cmd_data(0xB0, {0x01});
  delay(10);

  // 0xB1 (via CS)
  this->cmd_data(0xB1, {0x02});
  delay(10);

  return true;
}

void EPaperT133A01::command_cs1_(uint8_t value) {
  ESP_LOGV(TAG, "CS1 Command: 0x%02X", value);
  if (this->cs1_pin_ == nullptr) {
    ESP_LOGE(TAG, "CS1 pin not configured");
    return;
  }
  this->cs1_pin_->digital_write(false);  // Select CS1
  this->dc_pin_->digital_write(false);
  this->enable();
  this->write_byte(value);
  this->disable();
  this->cs1_pin_->digital_write(true);
}

void EPaperT133A01::cmd_data_cs1_(uint8_t command, const uint8_t *data, size_t length) {
  ESP_LOGV(TAG, "CS1 Command: 0x%02X, Length: %u", command, (unsigned) length);
  if (this->cs1_pin_ == nullptr) {
    ESP_LOGE(TAG, "CS1 pin not configured");
    return;
  }
  this->cs1_pin_->digital_write(false);  // Select CS1
  this->dc_pin_->digital_write(false);
  this->enable();
  this->write_byte(command);
  if (length > 0) {
    this->dc_pin_->digital_write(true);
    this->write_array(data, length);
  }
  this->disable();
  this->cs1_pin_->digital_write(true);
}

void EPaperT133A01::fill(Color color) {
  if (this->get_clipping().is_set()) {
    EPaperBase::fill(color);
    return;
  }
  auto pixel_color = color_to_index(color);
  this->buffer_.fill(pixel_color + (pixel_color << 4));
}

void EPaperT133A01::clear() { this->fill(COLOR_ON); }

void EPaperT133A01::draw_pixel_at(int x, int y, Color color) {
  if (!this->rotate_coordinates_(x, y))
    return;
  auto pixel_bits = color_to_index(color);
  uint32_t pixel_position = x + y * this->get_width_internal();
  uint32_t byte_position = pixel_position / 2;
  auto original = this->buffer_[byte_position];
  if ((pixel_position & 1) != 0) {
    this->buffer_[byte_position] = (original & 0xF0) | pixel_bits;
  } else {
    this->buffer_[byte_position] = (original & 0x0F) | (pixel_bits << 4);
  }
}

void EPaperT133A01::power_on() {
  ESP_LOGV(TAG, "Power on via CS1");
  this->command_cs1_(R04_PON);
}

void EPaperT133A01::power_off() {
  ESP_LOGV(TAG, "Power off via CS1");
  this->cmd_data_cs1_(R02_POF, {0x00});
}

void EPaperT133A01::refresh_screen(bool partial) {
  ESP_LOGV(TAG, "Refresh screen via CS1");
  // Display Refresh
  this->cmd_data_cs1_(R12_DRF, {0x01});
}

void EPaperT133A01::deep_sleep() {
  ESP_LOGV(TAG, "Deep sleep via CS1");
  this->cmd_data_cs1_(0x07, {0xA5});
}

bool HOT EPaperT133A01::transfer_data() {
  const uint32_t start_time = millis();
  const uint16_t bytes_per_half_row = this->width_ / 4;
  const uint16_t total_rows = this->height_;
  const uint16_t bytes_per_row = this->width_ / 2;
  uint8_t line_data[400];

  size_t half = this->current_data_index_;

  // --- CCSET: select color set before data transfer ---
  if (half == 0 && this->cs1_pin_ != nullptr) {
    this->cmd_data_cs1_(RE0_CCSET, {0x01});
    this->wait_for_idle_(true);
    delay(10);
  }

  // --- CS phase: left half of each row via CS (GPIO10) ---
  // T133A01 requires CS to stay LOW for the ENTIRE DTM data stream.
  // Toggling CS between chunks resets the controller's data pointer,
  // causing only the last chunk to be retained. Keep CS asserted
  // across timeout boundaries by NOT calling disable() on yield.
  if (half < total_rows) {
    if (half == 0) {
      if (this->cs1_pin_ != nullptr)
        this->cs1_pin_->digital_write(true);
      this->dc_pin_->digital_write(false);
      this->enable();
      this->write_byte(R10_DTM);
      this->dc_pin_->digital_write(true);
    }

    while (half < total_rows) {
      size_t buf_offset = half * bytes_per_row;
      for (uint16_t col = 0; col < bytes_per_half_row; col++) {
        uint8_t b = this->buffer_[buf_offset + col];
        line_data[col] = (remap_color(b >> 4) << 4) | remap_color(b & 0x0F);
      }
      this->write_array(line_data, bytes_per_half_row);
      half++;
      this->current_data_index_ = half;

      if (millis() - start_time > MAX_TRANSFER_TIME) {
        return false;
      }
    }
    ESP_LOGD(TAG, "CS phase done");
    this->disable();
  }

  // --- CS1 phase: right half of each row via CS1 (GPIO2) ---
  // Same continuous-transaction requirement as CS phase.
  // CS (GPIO10) is overridden HIGH after enable() so only CS1 receives data.
  if (half >= total_rows && half < total_rows * 2) {
    size_t cs1_row = half - total_rows;

    if (cs1_row == 0) {
      if (this->cs1_pin_ != nullptr)
        this->cs1_pin_->digital_write(false);
      gpio_set_level(GPIO_NUM_10, 1);
      this->enable();
      gpio_set_level(GPIO_NUM_10, 1);
      this->dc_pin_->digital_write(false);
      this->write_byte(R10_DTM);
      this->dc_pin_->digital_write(true);
    }

    while (half < total_rows * 2) {
      size_t row = half - total_rows;
      size_t buf_offset = row * bytes_per_row + bytes_per_half_row;
      for (uint16_t col = 0; col < bytes_per_half_row; col++) {
        uint8_t b = this->buffer_[buf_offset + col];
        line_data[col] = (remap_color(b >> 4) << 4) | remap_color(b & 0x0F);
      }
      this->write_array(line_data, bytes_per_half_row);
      half++;
      this->current_data_index_ = half;

      if (millis() - start_time > MAX_TRANSFER_TIME) {
        return false;
      }
    }
    ESP_LOGD(TAG, "CS1 phase done");
    if (this->cs1_pin_ != nullptr)
      this->cs1_pin_->digital_write(true);
    this->disable();
  }

  this->current_data_index_ = 0;
  return true;
}

void EPaperT133A01::dump_config() {
  EPaperBase::dump_config();
  LOG_PIN("  CS1 Pin: ", this->cs1_pin_);
  LOG_PIN("  Enable Pin: ", this->enable_pin_);
}

}  // namespace esphome::epaper_spi
