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
    this->static_ = false;
  }

  void set_data_static(const uint8_t *data, size_t len) {
    this->data_.static_data.ptr = data;
    this->data_.static_data.len = len;
    this->static_ = true;
  }

  void play(const Ts &...x) override {
    std::vector<uint8_t> data;
    if (this->static_) {
      data.assign(this->data_.static_data.ptr, this->data_.static_data.ptr + this->data_.static_data.len);
    } else {
      data = this->data_.func(x...);
    }
    this->parent_->transmit_packet(data);
  }

 protected:
  bool static_{true};
  union Data {
    std::vector<uint8_t> (*func)(Ts...);
    struct {
      const uint8_t *ptr;
      size_t len;
    } static_data;
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
