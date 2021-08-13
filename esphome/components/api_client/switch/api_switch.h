#pragma once

#include "esphome/core/component.h"
#include "../api_client_connection.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace api {

class ApiClientSwitch : public switch_::Switch, public Component {
 public:
  void dump_config() override;
  void set_api_key(uint32_t key) { this->key_ = key; }

  void set_api_parent(APIClientConnection *parent) { this->parent_ = parent; }

 protected:
  void write_state(bool state) override;

  APIClientConnection *parent_;
  uint32_t key_{0};
};

}  // namespace api
}  // namespace esphome
