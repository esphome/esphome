#pragma once
#include <string>
#include "esphome/components/api/custom_api_device.h"
#include "esphome/core/component.h"

namespace esphome {
namespace homeassistant {

class HomeAssistantComponent : public Component, public api::CustomAPIDevice {
 public:
  void setup() override{};
  void set_entity_id(const std::string &entity_id) { entity_id_ = entity_id; }
  std::string get_entity_id() { return entity_id_; }
  float get_setup_priority() const override { return setup_priority::LATE; }

 protected:
  std::string entity_id_;
  uint32_t ignore_update_until_ = 0;
  bool next_api_publish_ = false;
  bool can_update_from_api_();
  void ignore_api_updates_with_seconds_(int seconds);
};

}  // namespace homeassistant
}  // namespace esphome
