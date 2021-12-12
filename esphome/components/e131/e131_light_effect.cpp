#ifdef USE_ARDUINO

#include "e131.h"
#include "e131_light_effect.h"
#include "esphome/core/log.h"

namespace esphome {
namespace e131 {

static const char *const TAG = "e131_light_effect";
static const int MAX_DATA_SIZE = (sizeof(E131Packet::values) - 1);

E131LightEffect::E131LightEffect(const std::string &name) : LightEffect(name) {}

const std::string &E131LightEffect::get_name() { return LightEffect::get_name(); }

void E131LightEffect::start() {
  LightEffect::start();
  E131LightEffectBase::start();
}

void E131LightEffect::stop() {
  E131LightEffectBase::stop();
  LightEffect::stop();
}

void E131LightEffect::apply() {
  // ignore, it is run by `E131Component::update()`
}

int esphome::e131::E131LightEffect::get_universe_count() const { return 1; }

bool esphome::e131::E131LightEffect::process_(int universe, const E131Packet &packet) {
  // check if this is our universe and data are valid
  if (universe < first_universe_ || universe > get_last_universe())
    return false;

  int output_offset = (universe - first_universe_) * get_lights_per_universe();
  // limit amount of lights per universe and received
  int output_end = std::min(1, std::min(output_offset + get_lights_per_universe(), output_offset + packet.count - 1));
  const auto *input_data = packet.values + 1;

  ESP_LOGV(TAG, "Applying data for '%s' on %d universe, for %d-%d.", get_name().c_str(), universe, output_offset,
           output_end);

  auto call = this->state_->turn_on();

  switch (channels_) {
    case E131_MONO:
      if (output_offset < output_end) {
        call.set_red_if_supported(input_data[0] / 255.0);
        call.set_green_if_supported(input_data[0] / 255.0);
        call.set_blue_if_supported(input_data[0] / 255.0);
        call.set_white_if_supported(input_data[0] / 255.0);
        call.set_brightness_if_supported(input_data[0] / 255.0);
      }
      break;

    case E131_RGB:
      if (output_offset < output_end) {
        call.set_red_if_supported(input_data[0] / 255.0);
        call.set_green_if_supported(input_data[1] / 255.0);
        call.set_blue_if_supported(input_data[2] / 255.0);
        call.set_white_if_supported((input_data[0] + input_data[1] + input_data[2]) / 3.0 / 255.0);
        call.set_brightness_if_supported(std::max(input_data[0], std::max(input_data[1], input_data[2])) / 255.0);
      }
      break;

    case E131_RGBW:
      if (output_offset < output_end) {
        call.set_red_if_supported(input_data[0] / 255.0);
        call.set_green_if_supported(input_data[1] / 255.0);
        call.set_blue_if_supported(input_data[2] / 255.0);
        call.set_white_if_supported(input_data[3] / 255.0);
        call.set_brightness_if_supported(
            std::max(input_data[0], std::max(input_data[1], std::max(input_data[2], input_data[3]))) / 255.0);
      }
      break;
  }

  call.set_transition_length_if_supported(0);
  call.set_publish(false);
  call.set_save(false);

  call.perform();

  return true;
}

}  // namespace e131
}  // namespace esphome

#endif  // USE_ARDUINO
