#pragma once

#ifdef USE_ESP8266

#include "esphome/core/component.h"
#include "esphome/core/hal.h"

#include <vector>

#include "LwRx.h"
#include "LwTx.h"

namespace esphome::lightwaverf {

#ifdef USE_ESP8266

class LightWaveRF final : public PollingComponent {
 public:
  void set_pin(InternalGPIOPin *pin_tx, InternalGPIOPin *pin_rx) {
    pin_tx_ = pin_tx;
    pin_rx_ = pin_rx;
  }
  void update() override;
  void setup() override;
  void dump_config() override;
  void read_tx();
  void send_rx(const std::vector<uint8_t> &msg, uint8_t repeats, bool inverted, int u_sec);

 protected:
  void print_msg_(uint8_t *msg, uint8_t len);
  uint8_t msg_[10];
  uint8_t msglen_ = 10;
  InternalGPIOPin *pin_tx_;
  InternalGPIOPin *pin_rx_;
  LwRx lwrx_;
  LwTx lwtx_;
};

#endif
}  // namespace esphome::lightwaverf
#endif
