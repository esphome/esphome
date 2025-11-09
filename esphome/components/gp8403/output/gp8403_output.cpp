#include "gp8403_output.h"

#include "esphome/core/log.h"

namespace esphome {
namespace gp8403 {

static const char *const TAG = "gp8403.output";

void GP8403Output::dump_config() {
  ESP_LOGCONFIG(TAG,
                "GP8403 Output:\n"
                "  Channel: %u",
                this->channel_);
}

void GP8403Output::write_state(float state) { this->parent_->write_state(state, this->channel_); }

}  // namespace gp8403
}  // namespace esphome
