#include "template_time.h"
#include "esphome/core/log.h"

namespace esphome {
namespace template_ {

static const char *const TAG = "template.time";

void TemplateRealTimeClock::setup() {}

void TemplateRealTimeClock::update() {}

void TemplateRealTimeClock::dump_config() {
  ESP_LOGCONFIG(TAG, "template.time");

  ESP_LOGCONFIG(TAG, "  Timezone: '%s'", this->timezone_.c_str());
}

float TemplateRealTimeClock::get_setup_priority() const { return setup_priority::DATA; }
}  // namespace template_
}  // namespace esphome
