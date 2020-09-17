#pragma once

#include <utility>
#include <string>
#include <vector>

#include "esphome/core/defines.h"
#include "esphome/core/automation.h"
#include "esphome/core/base_automation.h"

#include "simple_types.h"
#include "component.h"
#include "peer.h"
#include "payload_getter.h"
#include "send_action.h"

namespace esphome {
namespace wifi_now {

class WifiNowSendActionBase {};

template<typename... Ts> class WifiNowSendAction : public Action<Ts...>, WifiNowSendActionBase {
 public:
  WifiNowSendAction(WifiNowComponent *component) : component_(component), peer_(nullptr) {}

  virtual const payload_t get_payload(Ts... x) {
    payload_t payload(250);
    payload.resize(0);

    for (auto payload_getter : this->payload_getters_) {
      payload_getter->get_payload(payload, x...);
    }
    return payload;
  }

  void play_complex(Ts... x) override {
    this->num_running_++;

    var_ = std::make_tuple(x...);
    varsub_ = std::make_tuple(x..., this);
    send_attempts_ = 0;

    send(x...);
  }

  void stop() override {
    this->fail_.stop();
    this->success_.stop();
  }

  bool is_running() override {
    return waitforcallback_ || this->fail_.is_running() || this->success_.is_running() || this->is_running_next_();
  }

  void play(Ts... x) override{
      /* everything is in play_complex */
  };

  void set_peer(WifiNowPeer *peer) { this->peer_ = peer; }

  const WifiNowPeer *get_peer() const { return peer_; }

  void set_servicekey(const servicekey_t servicekey) { this->servicekey_ = servicekey; }

  const servicekey_t get_servicekey() const { return this->servicekey_; }

  void set_payload_getters(const std::vector<WifiNowPayloadGetter<Ts...> *> &payload_getters) {
    payload_getters_ = payload_getters;
  }

  void add_on_fail(const std::vector<Action<Ts..., WifiNowSendAction<Ts...> *> *> &actions) {
    this->fail_.add_actions(actions);
    this->fail_.add_action(new LambdaAction<Ts..., WifiNowSendAction<Ts...> *>(
        [=](Ts... x, WifiNowSendAction<Ts...> *action) { this->play_next_(x...); }));
  }

  void add_on_success(const std::vector<Action<Ts..., WifiNowSendAction<Ts...> *> *> &actions) {
    this->success_.add_actions(actions);
    this->success_.add_action(new LambdaAction<Ts..., WifiNowSendAction<Ts...> *>(
        [=](Ts... x, WifiNowSendAction<Ts...> *action) { this->play_next_(x...); }));
  }

  const uint get_send_attempts() const { return this->send_attempts_; }

  void send() {
    waitforcallback_ = true;
    send_attempts_++;

    WifiNowPacket packet(this->peer_ ? this->peer_->get_bssid() : bssid_t(), this->servicekey_, get_payload());
    this->component_->send(packet, [=](bool b) { this->sendcallback_(b); });
  }

 protected:
  void sendcallback_(bool result) {
    if (result) {
      if (this->success_.empty()) {
        this->play_next_tuple_(this->var_);
      } else {
        this->success_.play_tuple(this->varsub_);
      }
    } else {
      if (this->fail_.empty()) {
        this->play_next_tuple_(this->var_);
      } else {
        this->fail_.play_tuple(this->varsub_);
      }
    }
    waitforcallback_ = false;
  }

  WifiNowComponent *component_;
  WifiNowPeer *peer_;
  servicekey_t servicekey_{};
  std::vector<WifiNowPayloadGetter<Ts...> *> payload_getters_;
  ActionList<Ts..., WifiNowSendAction<Ts...> *> fail_;
  ActionList<Ts..., WifiNowSendAction<Ts...> *> success_;

  std::tuple<Ts...> var_{};
  std::tuple<Ts..., WifiNowSendAction<Ts...> *> varsub_{};
  bool waitforcallback_{false};
  uint send_attempts_{0};
};

}  // namespace wifi_now
}  // namespace esphome
