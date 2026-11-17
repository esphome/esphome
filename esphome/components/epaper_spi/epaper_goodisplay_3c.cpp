#include "epaper_goodisplay_3c.h"

namespace esphome::epaper_spi {

void EPaperGoodDisplay3C::power_on() {}

void EPaperGoodDisplay3C::refresh_screen(bool partial) {
  EPaperWeAct3C::refresh_screen(partial);
  this->next_delay_ = REFRESH_START_MS;
}

}  // namespace esphome::epaper_spi
