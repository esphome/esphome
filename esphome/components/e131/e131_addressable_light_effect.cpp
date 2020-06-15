#include "e131.h"
#include "e131_addressable_light_effect.h"
#include "esphome/core/log.h"

namespace esphome {
namespace e131 {

static const char *TAG = "e131_addressable_light_effect";
static const int MAX_DATA_SIZE = (sizeof(E131Packet::values) - 1);

E131AddressableLightEffect::E131AddressableLightEffect(const std::string &name) : AddressableLightEffect(name) {}

int E131AddressableLightEffect::get_data_per_universe() const { return get_lights_per_universe() * channels_; }

int E131AddressableLightEffect::get_lights_per_universe() const { return MAX_DATA_SIZE / channels_; }

int E131AddressableLightEffect::get_first_universe() const { return first_universe_; }

int E131AddressableLightEffect::get_last_universe() const { return first_universe_ + get_universe_count() - 1; }

int E131AddressableLightEffect::get_universe_count() const {
  // Round up to lights_per_universe
  auto lights = get_lights_per_universe();
  return (get_addressable_()->size() + lights - 1) / lights;
}

void E131AddressableLightEffect::start() {
  AddressableLightEffect::start();

  if (this->e131_) {
    this->e131_->add_effect(this);
  }
}

void E131AddressableLightEffect::stop() {
  if (this->e131_) {
    this->e131_->remove_effect(this);
  }

  AddressableLightEffect::stop();
}

void E131AddressableLightEffect::apply(light::AddressableLight &it, const light::ESPColor &current_color) {
  // ignore, it is run by `E131Component::update()`
}

bool E131AddressableLightEffect::process_(int universe, const E131Packet &packet) {
  auto it = get_addressable_();

  // check if this is our universe and data are valid
  if (universe < first_universe_ || universe > get_last_universe())
    return false;

  int output_offset = (universe - first_universe_) * get_lights_per_universe();
  // limit amount of lights per universe and received
  int output_end = std::min(it->size(), std::min(output_offset + get_lights_per_universe(), packet.count - 1));
  auto input_data = packet.values + 1;

  ESP_LOGV(TAG, "Applying data for '%s' on %d universe, for %d-%d.", get_name().c_str(), universe, output_offset,
           output_end);

  switch (channels_) {
    case E131_MONO:
      for (; output_offset < output_end; output_offset++, input_data++) {
        auto output = (*it)[output_offset];
        output.set(light::ESPColor(input_data[0], input_data[0], input_data[0], input_data[0]));
      }
      break;

    case E131_RGB:
      for (; output_offset < output_end; output_offset++, input_data += 3) {
        auto output = (*it)[output_offset];
        output.set(light::ESPColor(input_data[0], input_data[1], input_data[2],
                                   (input_data[0] + input_data[1] + input_data[2]) / 3));
      }
      break;

    case E131_RGBW:
      for (; output_offset < output_end; output_offset++, input_data += 4) {
        auto output = (*it)[output_offset];
        output.set(light::ESPColor(input_data[0], input_data[1], input_data[2], input_data[3]));
      }
      break;
  }

  return true;
}

}  // namespace e131
}  // namespace esphome
