#include "epaper_spi_t133a01.h"
#include "colorconv.h"

#include "esphome/core/log.h"

namespace esphome::epaper_spi {

static constexpr const char *const TAG = "epaper_spi.t133a01";

// Native hardware color codes for this panel's 4-bit color values.
enum T133A01Color : uint8_t {
  BLACK = 0x00,
  WHITE = 0x01,
  YELLOW = 0x02,
  RED = 0x03,
  BLUE = 0x05,
  GREEN = 0x06,
};

// T133A01 register addresses not shared with EPaperDualCS.
static constexpr uint8_t R01_PWR = 0x01;
static constexpr uint8_t R05_BTST_N = 0x05;
static constexpr uint8_t R06_BTST_P = 0x06;
static constexpr uint8_t R50_CDI = 0x50;
static constexpr uint8_t R61_TRES = 0x61;
static constexpr uint8_t RA5_DCDC = 0xA5;
static constexpr uint8_t RE3_PWS = 0xE3;

uint8_t EPaperT133A01::color_to_native(Color color) {
  return color_to_bwyrgb<uint8_t>(color, BLACK, WHITE, YELLOW, RED, GREEN, BLUE);
}

bool EPaperT133A01::reset() {
  for (auto *enable_pin : this->enable_pins_) {
    enable_pin->digital_write(true);
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
 * The init sequence mirrors the Arduino GFX library's EPD_INIT() macro (T133A01_Defines.h).
 * Commands routed to CHIP_PRIMARY only leave CS1 deselected; commands routed to CHIP_BOTH
 * assert CS and CS1 together.
 */
bool EPaperT133A01::initialise(bool partial) {
  // 0x74 - panel config (CS only)
  this->write_command_to_chip_(0x74, {0x00, 0x0C, 0x0C, 0xD9, 0xDD, 0xDD, 0x15, 0x15, 0x55}, CHIP_PRIMARY);
  delay(10);

  // CMD66 - panel config (CS + CS1)
  this->write_command_to_chip_(REG_CMD66, CMD66_V, sizeof(CMD66_V), CHIP_BOTH);
  delay(10);

  // PSR - Panel Setting Register (CS + CS1)
  this->write_command_to_chip_(0x00, {0xDF, 0x69}, CHIP_BOTH);
  delay(10);

  // DCDC (CS only)
  this->write_command_to_chip_(RA5_DCDC, {0x44, 0x54, 0x00}, CHIP_PRIMARY);
  delay(10);

  // CDI (CS + CS1)
  this->write_command_to_chip_(R50_CDI, {0x37}, CHIP_BOTH);
  delay(10);

  // 0x60 (CS + CS1)
  this->write_command_to_chip_(0x60, {0x03, 0x03}, CHIP_BOTH);
  delay(10);

  // 0x86 (CS + CS1)
  this->write_command_to_chip_(0x86, {0x10}, CHIP_BOTH);
  delay(10);

  // PWS - Phase Width Setting (CS + CS1)
  this->write_command_to_chip_(RE3_PWS, {0x22}, CHIP_BOTH);
  delay(10);

  // TRES - Resolution Setting (CS + CS1).
  // With width=1200, height=1600: first word = width = 1200, second word = height/2 = 800.
  this->write_command_to_chip_(R61_TRES,
                               {(uint8_t) (this->width_ >> 8), (uint8_t) (this->width_ & 0xFF),
                                (uint8_t) ((this->height_ / 2) >> 8), (uint8_t) ((this->height_ / 2) & 0xFF)},
                               CHIP_BOTH);
  delay(10);

  // PWR - Power Setting (CS only)
  this->write_command_to_chip_(R01_PWR, {0x0F, 0x00, 0x28, 0x2C, 0x28, 0x38}, CHIP_PRIMARY);
  delay(10);

  // 0xB6 (CS only)
  this->write_command_to_chip_(0xB6, {0x07}, CHIP_PRIMARY);
  delay(10);

  // BTST_P (CS only)
  this->write_command_to_chip_(R06_BTST_P, {0xE0, 0x20}, CHIP_PRIMARY);
  delay(10);

  // 0xB7 (CS only)
  this->write_command_to_chip_(0xB7, {0x01}, CHIP_PRIMARY);
  delay(10);

  // BTST_N (CS only)
  this->write_command_to_chip_(R05_BTST_N, {0xE0, 0x20}, CHIP_PRIMARY);
  delay(10);

  // 0xB0 (CS only)
  this->write_command_to_chip_(0xB0, {0x01}, CHIP_PRIMARY);
  delay(10);

  // 0xB1 (CS only)
  this->write_command_to_chip_(0xB1, {0x02}, CHIP_PRIMARY);
  delay(10);

  return true;
}

void EPaperT133A01::power_on() {
  ESP_LOGV(TAG, "Power on");
  this->write_command_to_chip_(REG_PON, CHIP_BOTH);
}

void EPaperT133A01::power_off() {
  ESP_LOGV(TAG, "Power off");
  this->write_command_to_chip_(REG_POF, {0x00}, CHIP_BOTH);
}

void EPaperT133A01::refresh_screen(bool partial) {
  ESP_LOGV(TAG, "Refresh screen");
  this->write_command_to_chip_(REG_DRF, {0x01}, CHIP_BOTH);
}

void EPaperT133A01::deep_sleep() {
  ESP_LOGV(TAG, "Deep sleep");
  this->write_command_to_chip_(0x07, {0xA5}, CHIP_BOTH);
}

}  // namespace esphome::epaper_spi
