#include "epaper_uc8179_bwr.h"
#include "esphome/core/log.h"

namespace esphome::epaper_spi {

static constexpr const char *const TAG = "epaper_spi.uc8179_bwr";

bool EPaperUC8179BWR::initialise(bool partial) {
  EPaperBase::initialise(partial);  // send the model init sequence
  ESP_LOGV(TAG, "Power on");
  // Power on before the data transfer; the state machine busy-waits for it before TRANSFER_DATA
  this->command(0x04);
  // Give the busy line time to assert before the state machine polls it
  this->next_delay_ = 100;
  return true;
}

void EPaperUC8179BWR::power_on() {
  // Power-on is sent at the end of initialise() instead, so that it comes before the data transfer
}

void EPaperUC8179BWR::refresh_screen(bool /* partial */) {
  ESP_LOGV(TAG, "Refresh");
  this->command(0x12);
  this->next_delay_ = 100;
}

void EPaperUC8179BWR::power_off() {
  ESP_LOGV(TAG, "Power off");
  this->command(0x02);
}

void EPaperUC8179BWR::deep_sleep() {
  ESP_LOGV(TAG, "Deep sleep");
  this->cmd_data(0x07, {0xA5});
}

}  // namespace esphome::epaper_spi
