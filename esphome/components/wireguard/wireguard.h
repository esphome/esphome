#pragma once

#include "esphome/core/component.h"

namespace esphome {
namespace wireguard {

class Wireguard : public PollingComponent {
 public:
   
  void setup() override;
  void update() override;
  void dump_config() override;

  float get_setup_priority() const override { return esphome::setup_priority::LATE; }

  void set_address(std::string address); 
  void set_private_key(std::string private_key);
  void set_peer_endpoint(std::string endpoint);
  void set_peer_key(std::string peer_key);
  void set_peer_port(uint16_t addresses);
  void set_preshared_key(std::string peer_key);

 private:
  std::string address_;
  std::string private_key_;
  std::string peer_endpoint_;
  std::string peer_key_;
  std::string preshared_key_;
  uint16_t peer_port_;
};


}  // namespace wireguard
}  // namespace esphome