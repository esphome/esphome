#include "text.h"
#include "esphome/core/defines.h"
#include "esphome/core/controller_registry.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome::text {

static const char *const TAG = "text";

void Text::publish_state(const std::string &state) { this->publish_state(state.data(), state.size()); }

void Text::publish_state(const char *state) { this->publish_state(state, strlen(state)); }

void Text::publish_state(const char *state, size_t len) {
  this->set_has_state(true);
  // Only assign if changed to avoid heap allocation
  if (len != this->state.size() || memcmp(state, this->state.data(), len) != 0) {
    this->state.assign(state, len);
  }
  if (this->traits.get_mode() == TEXT_MODE_PASSWORD) {
    ESP_LOGV(TAG, "'%s' >> " LOG_SECRET("'%s'"), this->get_name().c_str(), this->state.c_str());
  } else {
    ESP_LOGV(TAG, "'%s' >> '%s'", this->get_name().c_str(), this->state.c_str());
  }
  this->state_callback_.call(this->state);
#if defined(USE_TEXT) && defined(USE_CONTROLLER_REGISTRY)
  ControllerRegistry::notify_text_update(this);
#endif
}

}  // namespace esphome::text
