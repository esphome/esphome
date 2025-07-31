#include "event_emitter.h"

namespace esphome {
namespace event_emitter {

static const char *const TAG = "event_emitter";

void raise_event_emitter_full_error() {
  ESP_LOGE(TAG, "EventEmitter has reached the maximum number of listeners for event");
  ESP_LOGW(TAG, "Removing listener to make space for new listener");
}

}  // namespace event_emitter
}  // namespace esphome
