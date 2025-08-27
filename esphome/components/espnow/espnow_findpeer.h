#pragma once

#if defined(USE_ESP32X)

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/espnow/espnow_component.h"
#include "esphome/core/preferences.h"

#include <array>
#include <memory>
#include <queue>
#include <vector>
#include <mutex>
#include <map>

namespace esphome::espnow_findpeer {

enum ESPNowFindPeerState : uint8_t {
  FIND_PEER_IDLE = 0,
  FIND_PEER_WAITING,
  FIND_PEER_SUCCESS,
  FIND_PEER_FAILED,
};

class ESPNowFindPeer : public Parented<espnow::ESPNowComponent> {
 public:
  void loop();

  bool find(const uint8_t *mac, const uint8_t *data, size_t size);
  bool find(const uint8_t *mac) { return this->find(mac, nullptr, 0); }
  ESPNowFindPeerState get_status() { return this->current_status_; }
  void reset();

 protected:
  void send_ping_();

  uint8_t *current_address_{nullptr};
  uint8_t *current_data_{nullptr};
  size_t current_data_size_{0};

  ESPNowFindPeerState current_status_{FIND_PEER_IDLE};
  uint8_t start_channel_{0};
  uint8_t current_channel_{0};
  bool current_loop_next_{false};
}

template<typename... Ts>
class FindPeerction : public Action<Ts...>, public Parented<ESPNowFindPeer>, public Component {
  TEMPLATABLE_VALUE(peer_address_t, address);
  TEMPLATABLE_VALUE(std::vector<uint8_t>, data);

 public:
  void add_on_success(const std::vector<Action<Ts...> *> &actions) {
    this->success_.add_actions(actions);
    if (this->flags_.wait_for_sent) {
      this->success_.add_action(new LambdaAction<Ts...>([this](Ts... x) { this->play_next_(x...); }));
    }
  }
  void add_on_failed(const std::vector<Action<Ts...> *> &actions) {
    this->failed_.add_actions(actions);
    if (this->flags_.wait_for_sent) {
      this->failed_.add_action(new LambdaAction<Ts...>([this](Ts... x) {
        if (this->flags_.continue_on_error) {
          this->play_next_(x...);
        } else {
          this->stop_complex();
        }
      }));
    }
  }
  void set_wait_for_sent(bool wait_for_sent) { this->flags_.wait_for_sent = wait_for_sent; }
  void set_continue_on_error(bool continue_on_error) { this->flags_.continue_on_error = continue_on_error; }

  void loop() override {
    if (this->parent_->get_find_state() == FIND_PEER_FAILED) {
      this->parent_->find_reset();
      this->play_failed_();
    } else if (this->current_status_ == FIND_PEER_SUCCESS) {
      this->parent_->find_reset();
      this->play_success_();
    }
  }

  void play_complex(Ts... x) override {
    this->num_running_++;
    this->var_ = std::make_tuple(x...);

    peer_address_t address = this->address_.value(x...);
    std::vector<uint8_t> data = this->data_.value(x...);
    if (!this->parent_->find(address.data(), data.data(), data.size())) {
      this->play_failed_();
    }
  }

  void play(Ts... x) override {} /* ignore - see play_complex */

  void stop() override {
    this->sent_.stop();
    this->error_.stop();
  }

 protected:
  void play_failed_() {
    if (!this->failed_.empty()) {
      this->failed_.play(this->var_);
    } else if (this->flags_.wait_for_sent) {
      if (this->flags_.continue_on_error) {
        this->play_next_(this->var_);
      } else {
        this->stop_complex();
      }
    }
  }

  void play_success_() {
    if (!this->success_.empty()) {
      this->sent_.play(this->var_);
    } else if (this->flags_.wait_for_sent) {
      this->play_next_(this->var_);
    }
  }

  std::tuple<Ts...> var_{};
  ActionList<Ts...> success_;
  ActionList<Ts...> failed_;
  struct {
    uint8_t wait_for_sent : 1;      // Wait for the send operation to complete before continuing automation
    uint8_t continue_on_error : 1;  // Continue automation even if the send operation fails
    uint8_t reserved : 6;           // Reserved for future use
  } flags_{0};
};

}  // namespace esphome::espnow_findpeer
