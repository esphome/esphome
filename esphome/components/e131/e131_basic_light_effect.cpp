#include "e131_basic_light_effect.h"
#include "e131.h"
#include "esphome/core/log.h"

namespace esphome {
namespace e131 {

static const char *const TAG = "e131_basic_light_effect";
static const int MAX_DATA_SIZE = (sizeof(E131Packet::values) - 1);

E131BasicLightEffect::E131BasicLightEffect(const std::string &name) : E131AddressableLightEffect(name) {}

int E131BasicLightEffect::get_lights_per_universe() const { return 1; }

bool E131BasicLightEffect::process_(int universe, const E131Packet &packet) {
  // check if this is our universe and data are valid
  if (!is_universe_valid(universe))
    return false;

  auto *input_data = packet.values + 1;

  ESP_LOGV(TAG, "Applying data for '%s' on %d universe, for %" PRId32 "-%d.", get_name().c_str(), universe);

  auto call = this->state_->turn_on();

  switch (channels_) {
    case E131_MONO:
      call.set_red(input_data[0]);
      call.set_green(input_data[0]);
      call.set_blue(input_data[0]);
      break;

    case E131_RGB:
      ESP_LOGV(TAG, "Applying RGB values %d, %d, %d", input_data[0], input_data[1], input_data[2]);

      call.set_red(input_data[0]);
      call.set_green(input_data[1]);
      call.set_blue(input_data[2]);
      break;

    case E131_RGBW:
      call.set_red(input_data[0]);
      call.set_green(input_data[1]);
      call.set_blue(input_data[2]);
      call.set_white(input_data[3]);
      break;
  }

  call.set_publish(true);
  call.set_save(false);
  call.perform();

  return true;
}

}  // namespace e131
}  // namespace esphome
