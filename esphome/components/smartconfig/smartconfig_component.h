#pragma once

#include "esphome/components/wifi/wifi_component.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "esphome/core/log.h"

#if defined(USE_ESP32) || defined(USE_ESP8266)

namespace esphome {
namespace smartconfig {

using on_smartconfig_ready_cb_t = std::function<void()>;

class SmartConfigComponent : public Component {
 public:
  enum class SmartConfigState {
    SC_IDLE,
    SC_START,
    SC_READY,
    SC_DONE,
  };
  SmartConfigComponent();

  void loop() override;
#ifdef USE_ESP32
  void start();
#endif

  float get_setup_priority() const override;
  void config();
  void add_on_ready(on_smartconfig_ready_cb_t callback) { on_smartconfig_ready_.add(std::move(callback)); }

 protected:
  CallbackManager<void()> on_smartconfig_ready_;
  SmartConfigState state_ {SmartConfigState::SC_IDLE};
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern SmartConfigComponent *global_smartconfig_component;

class SmartConfigReadyTrigger : public Trigger<> {
 public:
  explicit SmartConfigReadyTrigger(SmartConfigComponent *&sc_component) {
    sc_component->add_on_ready([this]() {
      ESP_LOGD("smartconfig", "add_on_ready Smartconfig ready");
      this->trigger();
    });
  }
};

}  // namespace smartconfig
}  // namespace esphome

#endif
