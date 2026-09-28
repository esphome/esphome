// Reference: https://github.com/SolderedElectronics/Inkplate-Arduino-library (src/boards/Inkplate2)

#include "epaper_spi_inkplate2.h"
#include "esphome/core/log.h"

namespace esphome::epaper_spi {

static constexpr const char *const TAG = "epaper_spi.inkplate2";

void EPaperInkplate2::power_on() {
  // Power-on (0x04) leads the init sequence, so there is nothing to do here.
  ESP_LOGV(TAG, "Power on");
}

void EPaperInkplate2::power_off() {
  ESP_LOGV(TAG, "Power off");
  this->cmd_data(0x50, {0xF7});  // VCOM and data interval
  this->command(0x02);           // power off
}

void EPaperInkplate2::refresh_screen(bool partial) {
  ESP_LOGV(TAG, "Refresh screen");  // full refresh only; partial is unused
  // Send 0x11 then 0x12 back-to-back: 0x11 raises busy until the refresh finishes, so waiting for idle
  // between them (as the state machine does between states) would add a ~16s stall.
  this->cmd_data(0x11, {0x00});  // stop data transfer
  this->command(0x12);           // display refresh
}

void EPaperInkplate2::deep_sleep() {
  ESP_LOGV(TAG, "Deep sleep");
  this->cmd_data(0x07, {0xA5});
}

}  // namespace esphome::epaper_spi
