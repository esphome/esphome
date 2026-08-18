#include "esphome/core/defines.h"

#ifdef USE_ESP_IDF
#ifdef USE_BUTTON

#include "matter_action_button.h"
#include "matter_component.h"

#include "esphome/core/log.h"

#include <unordered_set>

namespace esphome {
namespace matter {

static const char *const TAG = "matter.action_button";

namespace {
// Function-local static — avoids the static initialization order fiasco
// that would bite us if this lived at namespace scope and got touched
// during a ctor before its own initializer had run. Every entry is a
// live MatterActionButton*; entries added/removed by the ctor/dtor and
// queried O(1) by MatterComponent::scan_and_register_buttons_.
std::unordered_set<const ::esphome::button::Button *> &instances_() {
  static std::unordered_set<const ::esphome::button::Button *> set;
  return set;
}
}  // namespace

MatterActionButton::MatterActionButton() { instances_().insert(this); }

MatterActionButton::~MatterActionButton() { instances_().erase(this); }

bool MatterActionButton::is_instance(const ::esphome::button::Button *btn) { return instances_().count(btn) > 0; }

void MatterActionButton::press_action() {
  if (this->matter_ == nullptr) {
    ESP_LOGE(TAG, "matter component pointer not wired — press ignored");
    return;
  }
  switch (this->action_) {
    case Action::OPEN_COMMISSIONING_WINDOW:
      ESP_LOGI(TAG, "opening enhanced commissioning window (%us) via '%s'",
               static_cast<unsigned>(this->timeout_seconds_), this->get_name().c_str());
      this->matter_->open_commissioning_window(this->timeout_seconds_);
      break;
    case Action::FACTORY_RESET:
      // esp_matter::factory_reset schedules the NVS wipe on the CHIP task and
      // then calls esp_restart — this call never returns from the reboot's
      // perspective. Log at WARN so it stands out in a captured trace.
      ESP_LOGW(TAG, "factory reset requested via '%s' — wiping fabric and rebooting", this->get_name().c_str());
      this->matter_->factory_reset();
      break;
  }
}

void MatterActionButton::dump_config() {
  ESP_LOGCONFIG(TAG, "Matter Action Button '%s':", this->get_name().c_str());
  const char *action_name = "unknown";
  switch (this->action_) {
    case Action::OPEN_COMMISSIONING_WINDOW:
      action_name = "open_commissioning_window";
      break;
    case Action::FACTORY_RESET:
      action_name = "factory_reset";
      break;
  }
  ESP_LOGCONFIG(TAG, "  Action: %s", action_name);
  if (this->action_ == Action::OPEN_COMMISSIONING_WINDOW) {
    ESP_LOGCONFIG(TAG, "  Timeout: %us", static_cast<unsigned>(this->timeout_seconds_));
  }
}

}  // namespace matter
}  // namespace esphome

#endif  // USE_BUTTON
#endif  // USE_ESP_IDF
