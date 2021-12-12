#ifdef USE_ARDUINO

#include "e131.h"
#include "e131_light_effect_base.h"

namespace esphome {
namespace e131 {

static const int MAX_DATA_SIZE = (sizeof(E131Packet::values) - 1);

E131LightEffectBase::E131LightEffectBase() {}

void E131LightEffectBase::start() {
  if (this->e131_) {
    this->e131_->add_effect(this);
  }
}

void E131LightEffectBase::stop() {
  if (this->e131_) {
    this->e131_->remove_effect(this);
  }
}

int E131LightEffectBase::get_data_per_universe() const { return get_lights_per_universe() * channels_; }

int E131LightEffectBase::get_lights_per_universe() const { return MAX_DATA_SIZE / channels_; }

int E131LightEffectBase::get_first_universe() const { return first_universe_; }

int E131LightEffectBase::get_last_universe() const { return first_universe_ + get_universe_count() - 1; }

}  // namespace e131
}  // namespace esphome

#endif  // USE_ARDUINO
