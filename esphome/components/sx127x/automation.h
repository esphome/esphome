#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/sx127x/sx127x.h"

namespace esphome {
namespace sx127x {

template<typename... Ts> class RunImageCalAction : public Action<Ts...>, public Parented<SX127x> {
 public:
  void play(const Ts &...x) override { this->parent_->run_image_cal(); }
};

template<typename... Ts> class SendPacketAction : public Action<Ts...>, public Parented<SX127x> {
 public:
  void set_data_template(std::vector<uint8_t> (*func)(Ts...)) {
    this->data_.func = func;
    this->len_ = -1;  // Sentinel value indicates template mode
  }

  void set_data_static(const uint8_t *data, size_t len) {
    this->data_.data = data;
    this->len_ = len;  // Length >= 0 indicates static mode
  }

  void play(const Ts &...x) override {
    std::vector<uint8_t> data;
    if (this->len_ >= 0) {
      // Static mode: copy from flash to vector
      data.assign(this->data_.data, this->data_.data + this->len_);
    } else {
      // Template mode: call function
      data = this->data_.func(x...);
    }
    this->parent_->transmit_packet(data);
  }

 protected:
  ssize_t len_{-1};  // -1 = template mode, >=0 = static mode with length
  union Data {
    std::vector<uint8_t> (*func)(Ts...);  // Function pointer (stateless lambdas)
    const uint8_t *data;                  // Pointer to static data in flash
  } data_;
};

template<typename... Ts> class SetModeTxAction : public Action<Ts...>, public Parented<SX127x> {
 public:
  void play(const Ts &...x) override { this->parent_->set_mode_tx(); }
};

template<typename... Ts> class SetModeRxAction : public Action<Ts...>, public Parented<SX127x> {
 public:
  void play(const Ts &...x) override { this->parent_->set_mode_rx(); }
};

template<typename... Ts> class SetModeSleepAction : public Action<Ts...>, public Parented<SX127x> {
 public:
  void play(const Ts &...x) override { this->parent_->set_mode_sleep(); }
};

template<typename... Ts> class SetModeStandbyAction : public Action<Ts...>, public Parented<SX127x> {
 public:
  void play(const Ts &...x) override { this->parent_->set_mode_standby(); }
};

}  // namespace sx127x
}  // namespace esphome
