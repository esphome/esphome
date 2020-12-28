#include "rs485_cover.h"
#include "esphome/core/log.h"

namespace esphome {
namespace rs485 {

static const char *TAG = "rs485.cover";

using namespace esphome::cover;

CoverTraits RS485Cover::get_traits() {
  auto traits = CoverTraits();
  traits.set_supports_position(true);
  return traits;
}

void RS485Cover::setup() {

}

void RS485Cover::dump_config() {
  ESP_LOGCONFIG(TAG, "RS485 Cover:");
}

}  // namespace rs485
}  // namespace esphome
