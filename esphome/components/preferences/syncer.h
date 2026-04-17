#pragma once

#include "esphome/core/preferences.h"
#include "esphome/core/component.h"

namespace esphome {
namespace preferences {

class IntervalSyncer final : public PollingComponent {
 public:
  void update() override { global_preferences->sync(); }
  void on_shutdown() override { global_preferences->sync(); }
};

}  // namespace preferences
}  // namespace esphome
