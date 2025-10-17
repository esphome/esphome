#include "esphome/core/log.h"
#include "hofman_energy_avarma.h"

namespace esphome {
namespace hofman_energy_avarma {

static const char *TAG = "hofman_energy_avarma.component";

void HofmanEnergyAvarmaComponent::setup() {}

void HofmanEnergyAvarmaComponent::loop() {}

void HofmanEnergyAvarmaComponent::dump_config() { ESP_LOGCONFIG(TAG, "Hofman Energy Avarma component"); }

}  // namespace hofman_energy_avarma
}  // namespace esphome
