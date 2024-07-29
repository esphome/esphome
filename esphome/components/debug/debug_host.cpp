#include "debug_component.h"
#ifdef USE_HOST
#include <climits>

namespace esphome {
namespace debug {

std::string DebugComponent::get_reset_reason_() { return ""; }

uint32_t DebugComponent::get_free_heap_() { return INT_MAX; }

void DebugComponent::get_device_info_(std::string &device_info) {}

void DebugComponent::update_platform_() {}

}  // namespace debug
}  // namespace esphome
#endif
