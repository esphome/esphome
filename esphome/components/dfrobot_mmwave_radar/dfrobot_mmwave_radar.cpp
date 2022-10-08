#include "dfrobot_mmwave_radar.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dfrobot_mmwave_radar {

static const char *const TAG = "dfrobot_mmwave_radar";

void DfrobotMmwaveRadarComponent::dump_config() {
    ESP_LOGCONFIG(TAG, "Range minimum: %f m", range_min_);
    ESP_LOGCONFIG(TAG, "Range maximum: %f m", range_max_);
    ESP_LOGCONFIG(TAG, "Delay after detect: %f s", delay_after_detect_);
    ESP_LOGCONFIG(TAG, "Delay after disappear: %f s", delay_after_disappear_);
}

}  // namespace dfrobot_mmwave_radar
}  // namespace esphome
