#include "lt_component.h"

#ifdef USE_LIBRETINY

#include "esphome/core/log.h"

namespace esphome {
namespace libretiny {

static const char *const TAG = "lt.component";

// TODO: Remove after testing CI memory impact for BK72XX
static uint8_t test_static_buffer[256];  // Test static RAM impact

static void test_memory_impact_function() {
  // Test code size impact
  for (size_t i = 0; i < sizeof(test_static_buffer); i++) {
    test_static_buffer[i] = static_cast<uint8_t>(i ^ 0xAA);
  }
}

void LTComponent::dump_config() {
  // TODO: Remove after testing CI memory impact for BK72XX
  test_memory_impact_function();

  ESP_LOGCONFIG(TAG,
                "LibreTiny:\n"
                "  Version: %s\n"
                "  Loglevel: %u",
                LT_BANNER_STR + 10, LT_LOGLEVEL);

#ifdef USE_TEXT_SENSOR
  if (this->version_ != nullptr) {
    this->version_->publish_state(LT_BANNER_STR + 10);
  }
#endif  // USE_TEXT_SENSOR
}

float LTComponent::get_setup_priority() const { return setup_priority::LATE; }

}  // namespace libretiny
}  // namespace esphome

#endif  // USE_LIBRETINY
