#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace ota {

enum OTAState { OTA_COMPLETED = 0, OTA_STARTED, OTA_IN_PROGRESS, OTA_ERROR };

class OTAComponent : public Component {
 public:
  virtual bool should_enter_safe_mode(uint8_t num_attempts, uint32_t enable_time){ return false;}
  /// Set to true if the next startup will enter safe mode
  virtual void set_safe_mode_pending(const bool &pending){}
  virtual bool get_safe_mode_pending() {return false;}

#ifdef USE_OTA_STATE_CALLBACK
  void add_on_state_callback(std::function<void(OTAState, float, uint8_t)> &&callback);
#endif
protected:
#ifdef USE_OTA_STATE_CALLBACK
  CallbackManager<void(OTAState, float, uint8_t)> state_callback_{};
#endif
};

extern OTAComponent *global_ota_component;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace ota
}  // namespace esphome
