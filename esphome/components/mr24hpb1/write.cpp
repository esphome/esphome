#include "mr24hpb1.h"
#include "constants.h"

namespace esphome {
namespace mr24hpb1 {
void MR24HPB1Component::write_threshold_gear_(uint8_t gear) {
  if (gear < 1 || gear > 10) {
    ESP_LOGE(TAG, "Threshold gear value out of range! 1-10, got: %d", gear);
    return;
  }

  ESP_LOGD(TAG, "Setting threshold gear to %d", gear);
  std::vector<uint8_t> data = {gear};
  this->write_packet_(COPY_ORDER, CO_SYSTEM_PARAMETERS, CO_SP_THRESHOLD_GEAR, data);
}

void MR24HPB1Component::write_scene_setting_(SceneSetting setting) {
  if (setting < SCENE_DEFAULT || setting > HOTEL) {
    ESP_LOGE(TAG, "Scene setting value out of range! Got: %d", setting);
    return;
  }

  ESP_LOGD(TAG, "Setting scene setting to %s", scene_setting_to_string(setting));
  std::vector<uint8_t> data = {static_cast<uint8_t>(setting)};
  this->write_packet_(COPY_ORDER, CO_SYSTEM_PARAMETERS, CO_SP_SCENE_SETTING, data);
}

void MR24HPB1Component::write_force_unoccupied_setting_(ForcedUnoccupied setting) {
  if (setting < ForcedUnoccupied::NONE || setting > ForcedUnoccupied::MIN_60) {
    ESP_LOGE(TAG, "Forced unoccupied value out of range! Got: %x", static_cast<uint8_t>(setting));
    return;
  }

  ESP_LOGD(TAG, "Setting forced unoccupied to %s", forced_unoccupied_to_string(setting));
  std::vector<uint8_t> data = {static_cast<uint8_t>(setting)};
  this->write_packet_(COPY_ORDER, CO_SYSTEM_PARAMETERS, CO_SP_FORCED_UNOCCUPIED_SETTING, data);
}
}  // namespace mr24hpb1
}  // namespace esphome
