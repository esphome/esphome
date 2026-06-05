#pragma once

#include "esphome/core/defines.h"
#ifdef USE_WIFI_TWT

#include <atomic>

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/wifi/wifi_component.h"

namespace esphome::wifi_twt {

class WiFiTWT : public Component, public wifi::WiFiIPStateListener, public wifi::WiFiConnectStateListener {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void on_ip_state(const network::IPAddresses &ips, const network::IPAddress &dns1,
                   const network::IPAddress &dns2) override;
  void on_wifi_connect_state(StringRef ssid, std::span<const uint8_t, 6> bssid) override;

  void start_twt();
  void stop_twt();

  // Platform event notification interface — called from the ESP-IDF event task.
  // Each method defers callback dispatch to the main loop via defer().
  void twt_setup_success(uint8_t flow_id);
  void twt_teardown_received(uint8_t flow_id);
  void twt_wakeup_received();

  // Configuration setters
  void set_wake_interval_ms(uint32_t ms) { this->wake_interval_ms_ = ms; }
  void set_wake_duration_ms(uint32_t ms) { this->wake_duration_ms_ = ms; }
  void set_setup_cmd(uint8_t cmd) { this->setup_cmd_ = cmd; }
  void set_flow_type(uint8_t flow_type) { this->flow_type_ = flow_type; }
  void set_auto_setup(bool auto_setup) { this->auto_setup_ = auto_setup; }

  // Callback registration — templated to accept both std::function and forwarder structs
  template<typename F> void add_on_start_callback(F &&cb) { this->start_callback_.add(std::forward<F>(cb)); }
  template<typename F> void add_on_stop_callback(F &&cb) { this->stop_callback_.add(std::forward<F>(cb)); }
  template<typename F> void add_on_wakeup_callback(F &&cb) { this->wakeup_callback_.add(std::forward<F>(cb)); }

 protected:
  // Config — human-readable values; platform impl converts to wire format in start_twt()
  uint32_t wake_interval_ms_{15000};
  uint32_t wake_duration_ms_{10};
  uint8_t setup_cmd_{0};  // ordinals match wifi_twt_setup_cmds_t: REQUEST=0, SUGGEST=1, DEMAND=2
  uint8_t flow_type_{0};  // 0=announced, 1=unannounced
  bool auto_setup_{true};

  // Written by event task (twt_setup_success / twt_teardown_received), read by main task.
  std::atomic<uint8_t> active_flow_id_{UINT8_MAX};

  // esp_event_handler_instance_t handles (void * per ESP-IDF typedef); stored to allow
  // unregistering on partial setup failure.
  void *itwt_setup_handle_{nullptr};
  void *itwt_teardown_handle_{nullptr};
  void *twt_wakeup_handle_{nullptr};
  void *itwt_probe_handle_{nullptr};

  LazyCallbackManager<void()> start_callback_;
  LazyCallbackManager<void()> stop_callback_;
  LazyCallbackManager<void()> wakeup_callback_;
};

}  // namespace esphome::wifi_twt

#endif  // USE_WIFI_TWT
