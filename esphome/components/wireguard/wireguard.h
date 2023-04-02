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

  float get_setup_priority() const override { return esphome::setup_priority::LATE; }

  void set_address(std::string address);
  void set_netmask(std::string netmask);
  void set_private_key(std::string private_key);
  void set_peer_endpoint(std::string endpoint);
  void set_peer_key(std::string peer_key);
  void set_peer_port(uint16_t port);
  void set_preshared_key(std::string key);

#ifdef USE_ESP_IDF
  void set_keepalive(uint16_t seconds);
  void set_srctime(time::RealTimeClock* srctime);
#endif

 private:
  std::string address_;
  std::string netmask_;
  std::string private_key_;
  std::string peer_endpoint_;
  std::string peer_key_;
  std::string preshared_key_;
  uint16_t peer_port_;

#ifdef USE_ESP_IDF
  uint16_t keepalive_;
  time::RealTimeClock* srctime_;

  wireguard_config_t wg_config = ESP_WIREGUARD_CONFIG_DEFAULT();
  wireguard_ctx_t wg_ctx = ESP_WIREGUARD_CONTEXT_DEFAULT();

  esp_err_t wg_initialized = ESP_FAIL;
  esp_err_t wg_connected = ESP_FAIL;
  esp_err_t wg_aborted = ESP_FAIL;
  time_t wg_last_peer_up = 0;
  bool wg_peer_up = false;
  char wg_tmp_buffer[34];

  void start_connection();
#endif
};


}  // namespace wireguard
}  // namespace esphome

// vim: tabstop=2 shiftwidth=2 expandtab
