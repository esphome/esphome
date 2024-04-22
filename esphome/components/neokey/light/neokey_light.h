#pragma once

#include "../neokey.h"
#include "esphome/components/light/addressable_light.h"

namespace esphome {
namespace neokey {

static const uint8_t NUM_BYTES_PER_LED = 3;

class NeoKeyLight : public light::AddressableLight {
 public:
  void setup() override;
  void set_neokey(NeoKeyComponent *neokey) { neokey_ = neokey; }
  int32_t size() const override { return this->neokey_ == nullptr ? 0 : this->neokey_->hub_.pixels.numPixels(); }
  void write_state(light::LightState *state) override;
  void clear_effect_data() override {
    for (int i = 0; i < this->size(); i++)
      this->effect_data_[i] = 0;
  }
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::RGB});
    return traits;
  }

 protected:
  NeoKeyComponent *neokey_;

  uint8_t *buf_{nullptr};
  uint8_t *effect_data_{nullptr};

  light::ESPColorView get_view_internal(int32_t index) const override {
    uint8_t *base = this->buf_ + (NUM_BYTES_PER_LED * index);

    return light::ESPColorView(base + 0, base + 1, base + 2, nullptr, this->effect_data_ + index, &this->correction_);
  }
};

}  // namespace neokey
}  // namespace esphome
