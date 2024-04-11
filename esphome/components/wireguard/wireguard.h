#pragma once

#include <ctime>
#include <vector>
#include <tuple>

#include "esphome/core/component.h"
#include "esphome/components/time/real_time_clock.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

#include <esp_wireguard.h>

namespace esphome {
namespace wireguard {

/// Main Wireguard component class.
class Wireguard : public PollingComponent {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;
  void on_shutdown() override;
  bool can_proceed() override;

  float get_setup_priority() const override { return esphome::setup_priority::BEFORE_CONNECTION; }

  void set_address(const std::string &address);
  void set_netmask(const std::string &netmask);
  void set_private_key(const std::string &key);
  void set_peer_endpoint(const std::string &endpoint);
  void set_peer_public_key(const std::string &key);
  void set_peer_port(uint16_t port);
  void set_preshared_key(const std::string &key);

  void add_allowed_ip(const std::string &ip, const std::string &netmask);

  void set_keepalive(uint16_t seconds);
  void set_reboot_timeout(uint32_t seconds);
  void set_srctime(time::RealTimeClock *srctime);

#ifdef USE_BINARY_SENSOR
  void set_status_sensor(binary_sensor::BinarySensor *sensor);
  void set_enabled_sensor(binary_sensor::BinarySensor *sensor);
#endif

#ifdef USE_SENSOR
  void set_handshake_sensor(sensor::Sensor *sensor);
#endif

#ifdef USE_TEXT_SENSOR
  void set_address_sensor(text_sensor::TextSensor *sensor);
#endif

  /// Block the setup step until peer is connected.
  void disable_auto_proceed();

  /// Enable the WireGuard component.
  void enable();

  /// Stop any running connection and disable the WireGuard component.
  void disable();

  /// Publish the enabled state if the enabled binary sensor is configured.
  void publish_enabled_state();

  /// Return if the WireGuard component is or is not enabled.
  bool is_enabled();

  bool is_peer_up() const;
  time_t get_latest_handshake() const;

 protected:
  std::string address_;
  std::string netmask_;
  std::string private_key_;
  std::string peer_endpoint_;
  std::string peer_public_key_;
  std::string preshared_key_;

  std::vector<std::tuple<std::string, std::string>> allowed_ips_;

  uint16_t peer_port_;
  uint16_t keepalive_;
  uint32_t reboot_timeout_;

  time::RealTimeClock *srctime_;

#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *status_sensor_ = nullptr;
  binary_sensor::BinarySensor *enabled_sensor_ = nullptr;
#endif

#ifdef USE_SENSOR
  sensor::Sensor *handshake_sensor_ = nullptr;
#endif

#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *address_sensor_ = nullptr;
#endif

  /// Set to false to block the setup step until peer is connected.
  bool proceed_allowed_ = true;

  /// When false the wireguard link will not be established
  bool enabled_ = true;

  wireguard_config_t wg_config_ = ESP_WIREGUARD_CONFIG_DEFAULT();
  wireguard_ctx_t wg_ctx_ = ESP_WIREGUARD_CONTEXT_DEFAULT();

  esp_err_t wg_initialized_ = ESP_FAIL;
  esp_err_t wg_connected_ = ESP_FAIL;

  /// The last time the remote peer become offline.
  uint32_t wg_peer_offline_time_ = 0;

  /** \brief The latest saved handshake.
   *
   * This is used to save (and log) the latest completed handshake even
   * after a full refresh of the wireguard keys (for example after a
   * stop/start connection cycle).
   */
  time_t latest_saved_handshake_ = 0;

  void start_connection_();
  void stop_connection_();
};

// These are used for possibly long DNS resolution to temporarily suspend the watchdog
void suspend_wdt();
void resume_wdt();

/// Strip most part of the key only for secure printing
std::string mask_key(const std::string &key);

/// Condition to check if remote peer is online.
template<typename... Ts> class WireguardPeerOnlineCondition : public Condition<Ts...>, public Parented<Wireguard> {
 public:
  bool check(Ts... x) override { return this->parent_->is_peer_up(); }
};

/// Condition to check if Wireguard component is enabled.
template<typename... Ts> class WireguardEnabledCondition : public Condition<Ts...>, public Parented<Wireguard> {
 public:
  bool check(Ts... x) override { return this->parent_->is_enabled(); }
};

/// Action to enable Wireguard component.
template<typename... Ts> class WireguardEnableAction : public Action<Ts...>, public Parented<Wireguard> {
 public:
  void play(Ts... x) override { this->parent_->enable(); }
};

/// Action to disable Wireguard component.
template<typename... Ts> class WireguardDisableAction : public Action<Ts...>, public Parented<Wireguard> {
 public:
  void play(Ts... x) override { this->parent_->disable(); }
};

}  // namespace wireguard
}  // namespace esphome
