#include "epaper_waveshare_bwr.h"

namespace esphome::epaper_spi {

void EPaperWaveshareBWR::power_on() {
  this->cmd_data(0x01, {0x07, 0x17, 0x3F, 0x3F});  // POWER SETTING
  this->command(0x04);                             // POWER ON
}

void EPaperWaveshareBWR::refresh_screen(bool /*partial*/) {
  this->command(0x12);  // DISPLAY REFRESH
}

void EPaperWaveshareBWR::power_off() {
  this->command(0x02);  // POWER OFF
}

void EPaperWaveshareBWR::deep_sleep() {
  this->cmd_data(0x07, {0xA5});  // DEEP SLEEP with check code
}

}  // namespace esphome::epaper_spi
