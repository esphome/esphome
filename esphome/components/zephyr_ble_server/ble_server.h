#pragma once

#include "esphome/core/component.h"

namespace esphome {
namespace zephyr_ble_server {

class BLEServer : public Component {
 public:
  void setup() override;
};

}  // namespace zephyr_ble_server
}  // namespace esphome
