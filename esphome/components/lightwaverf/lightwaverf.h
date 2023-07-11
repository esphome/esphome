#pragma once

#ifdef USE_ESP8266

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/automation.h"

#include <vector>

#include "LwRx.h"
#include "LwTx.h"

namespace esphome {
namespace lightwaverf {

#ifdef USE_ESP8266

class LightWaveRF : public PollingComponent {
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

template<typename... Ts> class SendRawAction : public Action<Ts...> {
 public:
  SendRawAction(LightWaveRF *parent) : parent_(parent){};
  TEMPLATABLE_VALUE(int, repeat);
  TEMPLATABLE_VALUE(int, inverted);
  TEMPLATABLE_VALUE(int, pulse_length);
  TEMPLATABLE_VALUE(std::vector<uint8_t>, code);

  void set_repeats(const int &data) { repeat_ = data; }
  void set_inverted(const int &data) { inverted_ = data; }
  void set_pulse_length(const int &data) { pulse_length_ = data; }
  void set_data(const std::vector<uint8_t> &data) { code_ = data; }

  void play(Ts... x) {
    int repeats = this->repeat_.value(x...);
    int inverted = this->inverted_.value(x...);
    int pulse_length = this->pulse_length_.value(x...);
    std::vector<uint8_t> msg = this->code_.value(x...);

    this->parent_->send_rx(msg, repeats, inverted, pulse_length);
  }

 protected:
  LightWaveRF *parent_;
};

#endif
}  // namespace lightwaverf
}  // namespace esphome
#endif
