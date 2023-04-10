#pragma once

#include "esphome/core/component.h"

#ifdef USE_ESP_IDF
#include "esphome/components/time/real_time_clock.h"
#include "esp_wireguard.h"
#endif

namespace esphome {
namespace wireguard {

class Wireguard : public PollingComponent {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  void on_shutdown() override;

  float get_setup_priority() const override { return esphome::setup_priority::LATE; }

  void set_address(const std::string address);
  void set_netmask(const std::string netmask);
  void set_private_key(const std::string key);
  void set_peer_endpoint(const std::string endpoint);
  void set_peer_public_key(const std::string key);
  void set_peer_port(const uint16_t port);
  void set_preshared_key(const std::string key);

#ifdef USE_ESP_IDF
  void set_keepalive(const uint16_t seconds);
  void set_srctime(time::RealTimeClock* srctime);
  bool is_peer_up() const { return wg_peer_up_; }
  time_t get_last_peer_up() const { return wg_last_peer_up_; }
#endif

 private:
  std::string address_;
  std::string netmask_;
  std::string private_key_;
  std::string peer_endpoint_;
  std::string peer_public_key_;
  std::string preshared_key_;
  uint16_t peer_port_;

#ifdef USE_ESP_IDF
  uint16_t keepalive_;
  time::RealTimeClock* srctime_;

  wireguard_config_t wg_config_ = ESP_WIREGUARD_CONFIG_DEFAULT();
  wireguard_ctx_t wg_ctx_ = ESP_WIREGUARD_CONTEXT_DEFAULT();

  esp_err_t wg_initialized_ = ESP_FAIL;
  esp_err_t wg_connected_ = ESP_FAIL;
  time_t wg_last_peer_up_ = 0;
  bool wg_peer_up_ = false;

  void start_connection_();
#endif
};

}  // namespace wireguard
}  // namespace esphome

// vim: tabstop=2 shiftwidth=2 expandtab
