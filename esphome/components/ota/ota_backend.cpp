#include "ota_backend.h"

namespace esphome::ota {

bool version_is_older(const char *candidate, const char *reference) {
  if (candidate == nullptr || reference == nullptr)
    return false;
  while (true) {
    uint32_t a = 0;
    while (*candidate >= '0' && *candidate <= '9') {
      a = a * 10 + static_cast<uint32_t>(*candidate - '0');
      candidate++;
    }
    uint32_t b = 0;
    while (*reference >= '0' && *reference <= '9') {
      b = b * 10 + static_cast<uint32_t>(*reference - '0');
      reference++;
    }
    if (a != b)
      return a < b;
    // Components equal so far; advance past a single separator on each side.
    const bool a_more = (*candidate == '.');
    const bool b_more = (*reference == '.');
    if (a_more)
      candidate++;
    if (b_more)
      reference++;
    if (!a_more && !b_more)
      return false;  // Both strings exhausted with all components equal.
  }
}

#ifdef USE_OTA_STATE_LISTENER
OTAGlobalCallback *global_ota_callback{nullptr};  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

OTAGlobalCallback *get_global_ota_callback() {
  if (global_ota_callback == nullptr) {
    global_ota_callback = new OTAGlobalCallback();  // NOLINT(cppcoreguidelines-owning-memory)
  }
  return global_ota_callback;
}

void OTAComponent::notify_state_deferred_(OTAState state, float progress, uint8_t error) {
  // Pack state, error, and progress into a single uint32_t so the lambda
  // captures only [this, packed] (8 bytes) — fits in std::function SBO.
  // Layout: [state:8][error:8][progress_fixed:16] where progress is 0–10000 (0.01% resolution)
  static_assert(OTA_ERROR <= 0xFF, "OTAState must fit in 8 bits for packing");
  uint32_t packed = (static_cast<uint32_t>(state) << 24) | (static_cast<uint32_t>(error) << 16) |
                    static_cast<uint16_t>(progress * 100.0f);
  this->defer([this, packed]() {
    this->notify_state_(static_cast<OTAState>(packed >> 24), static_cast<float>(packed & 0xFFFF) / 100.0f,
                        static_cast<uint8_t>(packed >> 16));
  });
}

void OTAComponent::notify_state_(OTAState state, float progress, uint8_t error) {
  for (auto *listener : this->state_listeners_) {
    listener->on_ota_state(state, progress, error);
  }
  get_global_ota_callback()->notify_ota_state(state, progress, error, this);
}
#endif

}  // namespace esphome::ota
