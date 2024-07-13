#include "event_emitter.h"

namespace esphome {

static const char *const TAG = "event_emitter";

void RaiseEventEmitterFullError(EventEmitterListenerID id) {
  ESP_LOGE(TAG, "EventEmitter has reached the maximum number of listeners for event");
  ESP_LOGW(TAG, "Removing listener with ID %ld to make space for new listener", id);
}

}  // namespace esphome
