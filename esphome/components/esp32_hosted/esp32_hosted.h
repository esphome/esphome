#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <atomic>
#include <utility>

#include <esp_event.h>

namespace esphome::esp32_hosted {

/** Tracks the link to the co-processor.
 *
 * esp_hosted brings the link up from its own task, often before setup() runs, and reports changes through ESP-IDF
 * events. This component hands them to the main loop, so the callbacks always run there. Changes that come faster
 * than one loop pass are merged into the latest state.
 */
class ESP32Hosted : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS; }

  /// True while the link to the co-processor is up.
  bool is_linked() const { return this->linked_; }

  /// Called with true when the link comes up and with false when it goes down. A link that is already up when the
  /// callback is added is not reported again, so check is_linked() as well.
  template<typename F> void add_on_link_callback(F &&callback) { this->link_callback_.add(std::forward<F>(callback)); }

 protected:
  static void event_handler_(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
  static void initial_state_task_(void *arg);
  /// Safe from any task. The main loop reports the change.
  void set_link_state_(bool linked);

  // Written by esp_hosted's event task and the initial state task, read by the main loop
  std::atomic<bool> link_state_{false};
  bool linked_{false};
  LazyCallbackManager<void(bool)> link_callback_;
};

}  // namespace esphome::esp32_hosted

#endif  // USE_ESP32
