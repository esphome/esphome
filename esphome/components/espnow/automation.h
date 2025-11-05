#pragma once

#ifdef USE_ESP32

#include "espnow_component.h"

#include "esphome/core/automation.h"
#include "esphome/core/base_automation.h"

namespace esphome::espnow {

template<typename... Ts> class SendAction : public Action<Ts...>, public Parented<ESPNowComponent> {
  TEMPLATABLE_VALUE(peer_address_t, address);
  TEMPLATABLE_VALUE(std::vector<uint8_t>, data);

 public:
  void add_on_sent(const std::initializer_list<Action<Ts...> *> &actions) {
    this->sent_.add_actions(actions);
    if (this->flags_.wait_for_sent) {
      this->sent_.add_action(new LambdaAction<Ts...>([this](Ts... x) { this->play_next_(x...); }));
    }
  }
  void add_on_error(const std::initializer_list<Action<Ts...> *> &actions) {
    this->error_.add_actions(actions);
    if (this->flags_.wait_for_sent) {
      this->error_.add_action(new LambdaAction<Ts...>([this](Ts... x) {
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

  void play_complex(const Ts &...x) override {
    this->num_running_++;
    send_callback_t send_callback = [this, x...](esp_err_t status) {
      if (status == ESP_OK) {
        if (!this->sent_.empty()) {
          this->sent_.play(x...);
        } else if (this->flags_.wait_for_sent) {
          this->play_next_(x...);
        }
      } else {
        if (!this->error_.empty()) {
          this->error_.play(x...);
        } else if (this->flags_.wait_for_sent) {
          if (this->flags_.continue_on_error) {
            this->play_next_(x...);
          } else {
            this->stop_complex();
          }
        }
      }
    };
    peer_address_t address = this->address_.value(x...);
    std::vector<uint8_t> data = this->data_.value(x...);
    esp_err_t err = this->parent_->send(address.data(), data, send_callback);
    if (err != ESP_OK) {
      send_callback(err);
    } else if (!this->flags_.wait_for_sent) {
      this->play_next_(x...);
    }
  }

  void play(const Ts &...x) override { /* ignore - see play_complex */
  }

  void stop() override {
    this->sent_.stop();
    this->error_.stop();
  }

 protected:
  ActionList<Ts...> sent_;
  ActionList<Ts...> error_;

  struct {
    uint8_t wait_for_sent : 1;      // Wait for the send operation to complete before continuing automation
    uint8_t continue_on_error : 1;  // Continue automation even if the send operation fails
    uint8_t reserved : 6;           // Reserved for future use
  } flags_{0};
};

template<typename... Ts> class AddPeerAction : public Action<Ts...>, public Parented<ESPNowComponent> {
  TEMPLATABLE_VALUE(peer_address_t, address);

 public:
  void play(const Ts &...x) override {
    peer_address_t address = this->address_.value(x...);
    this->parent_->add_peer(address.data());
  }
};

template<typename... Ts> class DeletePeerAction : public Action<Ts...>, public Parented<ESPNowComponent> {
  TEMPLATABLE_VALUE(peer_address_t, address);

 public:
  void play(const Ts &...x) override {
    peer_address_t address = this->address_.value(x...);
    this->parent_->del_peer(address.data());
  }
};

template<typename... Ts> class SetChannelAction : public Action<Ts...>, public Parented<ESPNowComponent> {
 public:
  TEMPLATABLE_VALUE(uint8_t, channel)
  void play(const Ts &...x) override {
    if (this->parent_->is_wifi_enabled()) {
      return;
    }
    this->parent_->set_wifi_channel(this->channel_.value(x...));
    this->parent_->apply_wifi_channel();
  }
};

class OnReceiveTrigger : public Trigger<const ESPNowRecvInfo &, const uint8_t *, uint8_t>,
                         public ESPNowReceivedPacketHandler {
 public:
  explicit OnReceiveTrigger(std::array<uint8_t, ESP_NOW_ETH_ALEN> address) : has_address_(true) {
    memcpy(this->address_, address.data(), ESP_NOW_ETH_ALEN);
  }

  explicit OnReceiveTrigger() : has_address_(false) {}

  bool on_received(const ESPNowRecvInfo &info, const uint8_t *data, uint8_t size) override {
    bool match = !this->has_address_ || (memcmp(this->address_, info.src_addr, ESP_NOW_ETH_ALEN) == 0);
    if (!match)
      return false;

    this->trigger(info, data, size);
    return false;  // Return false to continue processing other internal handlers
  }

 protected:
  bool has_address_{false};
  const uint8_t *address_[ESP_NOW_ETH_ALEN];
};
class OnUnknownPeerTrigger : public Trigger<const ESPNowRecvInfo &, const uint8_t *, uint8_t>,
                             public ESPNowUnknownPeerHandler {
 public:
  bool on_unknown_peer(const ESPNowRecvInfo &info, const uint8_t *data, uint8_t size) override {
    this->trigger(info, data, size);
    return false;  // Return false to continue processing other internal handlers
  }
};
class OnBroadcastedTrigger : public Trigger<const ESPNowRecvInfo &, const uint8_t *, uint8_t>,
                             public ESPNowBroadcastedHandler {
 public:
  explicit OnBroadcastedTrigger(std::array<uint8_t, ESP_NOW_ETH_ALEN> address) : has_address_(true) {
    memcpy(this->address_, address.data(), ESP_NOW_ETH_ALEN);
  }
  explicit OnBroadcastedTrigger() : has_address_(false) {}

  bool on_broadcasted(const ESPNowRecvInfo &info, const uint8_t *data, uint8_t size) override {
    bool match = !this->has_address_ || (memcmp(this->address_, info.src_addr, ESP_NOW_ETH_ALEN) == 0);
    if (!match)
      return false;

    this->trigger(info, data, size);
    return false;  // Return false to continue processing other internal handlers
  }

 protected:
  bool has_address_{false};
  const uint8_t *address_[ESP_NOW_ETH_ALEN];
};

}  // namespace esphome::espnow

#endif  // USE_ESP32
