#include "ade7953_spi.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ade7953_spi {

static const char *const TAG = "ade7953";

void ADE7953_spi::dump_config() {
  ESP_LOGCONFIG(TAG, "ADE7953_spi:");
  LOG_PIN("  CS Pin: ", this->cs_);
  ade7953_base::ADE7953::dump_config();
}


}  // namespace ade7953_spi
}  // namespace esphome