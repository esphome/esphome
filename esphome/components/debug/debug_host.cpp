#include "debug_component.h"
#ifdef USE_HOST
#include <climits>

namespace esphome {
namespace debug {

const char *DebugComponent::get_reset_reason_(std::span<char, RESET_REASON_BUFFER_SIZE> buffer) { return ""; }

const char *DebugComponent::get_wakeup_cause_(std::span<char, RESET_REASON_BUFFER_SIZE> buffer) { return ""; }

uint32_t DebugComponent::get_free_heap_() { return INT_MAX; }

size_t DebugComponent::get_device_info_(std::span<char, DEVICE_INFO_BUFFER_SIZE> buffer, size_t pos) { return pos; }

void DebugComponent::update_platform_() {}

}  // namespace debug
}  // namespace esphome
#endif
