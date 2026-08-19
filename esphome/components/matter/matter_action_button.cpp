#include "esphome/core/defines.h"

#ifdef USE_ESP_IDF
#ifdef USE_BUTTON

#include "matter_action_button.h"
#include "matter_component.h"

#include "esphome/core/log.h"

#include <algorithm>
#include <vector>

namespace esphome::matter {

static const char *const TAG = "matter.action_button";

namespace {
// Function-local static — avoids the static initialization order fiasco
// that would bite us if this lived at namespace scope and got touched
// during a ctor before its own initializer had run. Every entry is a live
// MatterActionButton*; entries added/removed by the ctor/dtor and queried
// via linear scan by MatterComponent::scan_and_register_buttons_. Typical
// device count is 0-3, and even a very large config has fewer than a dozen
// action buttons — a std::vector<Button*> is materially smaller than an
// unordered_set at those counts (the set carries ~2KB of hash-table
// machinery before it holds anything).
std::vector<const ::esphome::button::Button *> &instances() {
  static std::vector<const ::esphome::button::Button *> vec;
  return vec;
}
}  // namespace

MatterActionButton::MatterActionButton() { instances().push_back(this); }

MatterActionButton::~MatterActionButton() {
  auto &vec = instances();
  vec.erase(std::remove(vec.begin(), vec.end(), this), vec.end());
}

bool MatterActionButton::is_instance(const ::esphome::button::Button *btn) {
  const auto &vec = instances();
  return std::find(vec.begin(), vec.end(), btn) != vec.end();
}

void MatterActionButton::press_action() {
  if (this->matter_ == nullptr) {
    ESP_LOGE(TAG, "matter component pointer not wired — press ignored");
    return;
  }
  switch (this->action_) {
    case Action::ACTION_OPEN_COMMISSIONING_WINDOW:
      ESP_LOGI(TAG, "opening enhanced commissioning window (%us) via '%s'",
               static_cast<unsigned>(this->timeout_seconds_), this->get_name().c_str());
      this->matter_->open_commissioning_window(this->timeout_seconds_);
      break;
    case Action::ACTION_FACTORY_RESET:
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
  const char *action_name;
  switch (this->action_) {
    case Action::ACTION_OPEN_COMMISSIONING_WINDOW:
      action_name = "open_commissioning_window";
      break;
    case Action::ACTION_FACTORY_RESET:
      action_name = "factory_reset";
      break;
    default:
      action_name = "unknown";
      break;
  }
  ESP_LOGCONFIG(TAG, "  Action: %s", action_name);
  if (this->action_ == Action::ACTION_OPEN_COMMISSIONING_WINDOW) {
    ESP_LOGCONFIG(TAG, "  Timeout: %us", static_cast<unsigned>(this->timeout_seconds_));
  }
}

}  // namespace esphome::matter

#endif  // USE_BUTTON
#endif  // USE_ESP_IDF
