// Reference: https://github.com/SolderedElectronics/Inkplate-Arduino-library (src/boards/Inkplate6COLOR)

#include "epaper_spi_inkplate6color.h"
#include "colorconv.h"

#include "esphome/core/log.h"

namespace esphome::epaper_spi {

static constexpr const char *const TAG = "epaper_spi.inkplate6color";

// Native hardware color codes for this panel's 4-bit color values.
enum Inkplate6ColorHex : uint8_t {
  BLACK = 0,
  WHITE = 1,
  GREEN = 2,
  BLUE = 3,
  RED = 4,
  YELLOW = 5,
  ORANGE = 6,
};

uint8_t EPaperInkplate6Color::color_to_native(Color color) {
  return color_to_bwyrgbo<uint8_t>(color, BLACK, WHITE, YELLOW, RED, GREEN, BLUE, ORANGE);
}

void EPaperInkplate6Color::power_on() {
  ESP_LOGV(TAG, "Power on");
  this->command(0x04);
}

void EPaperInkplate6Color::power_off() {
  ESP_LOGV(TAG, "Power off");
  this->command(0x02);
}

void EPaperInkplate6Color::refresh_screen(bool partial) {
  ESP_LOGV(TAG, "Refresh");  // full refresh only; partial is unused
  this->cmd_data(0x12, {0x00});
}

void EPaperInkplate6Color::deep_sleep() {
  ESP_LOGV(TAG, "Deep sleep");
  this->cmd_data(0x07, {0xA5});
}

}  // namespace esphome::epaper_spi
