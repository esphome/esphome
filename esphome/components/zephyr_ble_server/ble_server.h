#pragma once
#ifdef USE_ZEPHYR
#include "esphome/core/component.h"

namespace esphome::zephyr_ble_server {

class BLEServer : public Component {
 public:
  void setup() override;
  void loop() override;
  void on_shutdown() override;

 protected:
  void resume_();
  bool suspended_ = false;
};

}  // namespace esphome::zephyr_ble_server
#endif
