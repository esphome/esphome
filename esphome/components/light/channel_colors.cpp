#include "channel_colors.h"

namespace esphome::light {

LightTraits ChannelColors::traits() const {
  LightTraits traits;
  if (this->has_white()) {
    traits.set_supported_color_modes({ColorMode::RGB_WHITE, ColorMode::WHITE});
  } else {
    traits.set_supported_color_modes({ColorMode::RGB});
  }
  return traits;
}

}  // namespace esphome::light
