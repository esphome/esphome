#include "epaper_spi_spectra_e6.h"
#include "colorconv.h"

#include "esphome/core/log.h"

namespace esphome::epaper_spi {
static constexpr const char *const TAG = "epaper_spi.6c";

// Native hardware color codes for this panel's 4-bit color values.
enum E6Color : uint8_t {
  BLACK = 0,
  WHITE = 1,
  YELLOW = 2,
  RED = 3,
  BLUE = 5,
  GREEN = 6,
};

uint8_t EPaperSpectraE6::color_to_native(Color color) {
  return color_to_bwyrgb<uint8_t>(color, BLACK, WHITE, YELLOW, RED, GREEN, BLUE);
}

void EPaperSpectraE6::power_on() {
  ESP_LOGV(TAG, "Power on");
  this->command(0x04);
}

void EPaperSpectraE6::power_off() {
  ESP_LOGV(TAG, "Power off");
  this->cmd_data(0x02, {0x00});
}

void EPaperSpectraE6::refresh_screen(bool partial) {
  ESP_LOGV(TAG, "Refresh");
  this->cmd_data(0x12, {0x00});
}

void EPaperSpectraE6::deep_sleep() {
  ESP_LOGV(TAG, "Deep sleep");
  this->cmd_data(0x07, {0xA5});
}
}  // namespace esphome::epaper_spi
