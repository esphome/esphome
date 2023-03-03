#include "i2s_audio.h"

#include "esphome/core/log.h"

namespace esphome {
namespace i2s_audio {

static const char *const TAG = "i2s_audio";

void I2SAudioComponent::setup() { ESP_LOGCONFIG(TAG, "Setting up I2S Audio..."); }

}  // namespace i2s_audio
}  // namespace esphome
