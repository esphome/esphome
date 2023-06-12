#pragma once

#ifdef USE_ESP32

#include <ctime>
#include <vector>
#include <tuple>

#include "esphome/core/component.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"

#include <esp_wireguard.h>

namespace esphome {
namespace wireguard {

class Wireguard : public PollingComponent {
 public:
  void setup() override;
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
  void set_status_sensor(binary_sensor::BinarySensor *sensor);
  void set_handshake_sensor(sensor::Sensor *sensor);

  /// Block the setup step until peer is connected.
  void disable_auto_proceed();

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
  binary_sensor::BinarySensor *status_sensor_ = nullptr;
  sensor::Sensor *handshake_sensor_ = nullptr;

  /// Set to false to block the setup step until peer is connected.
  bool proceed_allowed_ = true;

  wireguard_config_t wg_config_ = ESP_WIREGUARD_CONFIG_DEFAULT();
  wireguard_ctx_t wg_ctx_ = ESP_WIREGUARD_CONTEXT_DEFAULT();

  esp_err_t wg_initialized_ = ESP_FAIL;
  esp_err_t wg_connected_ = ESP_FAIL;

  /// The last time the remote peer become offline.
  uint32_t wg_peer_offline_time_ = 0;

  /** \brief The latest saved handshake.
   *
   * This is used to publish an update to `this->handshake_sensor_` only
   * when a new handshake is reported by `esp_wireguard_latest_handshake`.
   */
  time_t latest_saved_handshake_ = 0;

  void start_connection_();
};

// These are used for possibly long DNS resolution to temporarily suspend the watchdog
void suspend_wdt();
void resume_wdt();

}  // namespace wireguard
}  // namespace esphome

#endif

// vim: tabstop=2 shiftwidth=2 expandtab
