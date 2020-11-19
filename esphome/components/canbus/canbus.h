#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/optional.h"

namespace esphome {
namespace canbus {

enum Error : uint8_t {
  ERROR_OK = 0,
  ERROR_FAIL = 1,
  ERROR_ALLTXBUSY = 2,
  ERROR_FAILINIT = 3,
  ERROR_FAILTX = 4,
  ERROR_NOMSG = 5
};

enum CanSpeed : uint8_t {
  CAN_5KBPS,
  CAN_10KBPS,
  CAN_20KBPS,
  CAN_31K25BPS,
  CAN_33KBPS,
  CAN_40KBPS,
  CAN_50KBPS,
  CAN_80KBPS,
  CAN_83K3BPS,
  CAN_95KBPS,
  CAN_100KBPS,
  CAN_125KBPS,
  CAN_200KBPS,
  CAN_250KBPS,
  CAN_500KBPS,
  CAN_1000KBPS
};

/* valid bits in CAN ID for frame formats */
// static const uint32_t CAN_SFF_MASK = 0x000007FFUL; /* standard frame format (SFF) */
// static const uint32_t CAN_EFF_MASK = 0x1FFFFFFFUL; /* extended frame format (EFF) */
// static const uint32_t CAN_ERR_MASK = 0x1FFFFFFFUL; /* omit EFF, RTR, ERR flags */

class CanbusTrigger;
template<typename... Ts> class CanbusSendAction;

/* CAN payload length and DLC definitions according to ISO 11898-1 */
static const uint8_t CAN_MAX_DLC = 8;
static const uint8_t CAN_MAX_DLEN = 8;

struct CanFrame {
  bool ext_id = false;
  bool rtr = false;
  uint32_t can_id; /* 29 or 11 bit CAN_ID  */
  uint8_t can_dlc; /* frame payload length in byte (0 .. CAN_MAX_DLEN) */
  uint8_t data[CAN_MAX_DLEN] __attribute__((aligned(8)));
};

class Canbus : public Component {
 public:
  Canbus(){};
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void loop() override;

  void send_data(uint32_t can_id, bool can_ext_id, const std::vector<uint8_t> &data);
  void set_can_id(int can_id) { this->can_id_ = can_id; }
  void set_can_ext_id(bool can_ext_id) { this->can_ext_id_ = can_ext_id; }
  void set_bitrate(CanSpeed bit_rate) { this->bit_rate_ = bit_rate; }

  void add_trigger(CanbusTrigger *trigger);

 protected:
  template<typename... Ts> friend class CanbusSendAction;
  std::vector<CanbusTrigger *> triggers_{};
  uint32_t can_id_;
  bool can_ext_id_;
  CanSpeed bit_rate_;

  virtual bool setup_internal();
  virtual Error send_message(struct CanFrame *frame);
  virtual Error read_message(struct CanFrame *frame);
};

template<typename... Ts> class CanbusSendAction : public Action<Ts...>, public Parented<Canbus> {
 public:
  void set_data_template(const std::function<std::vector<uint8_t>(Ts...)> func) {
    this->data_func_ = func;
    this->static_ = false;
  }
  void set_data_static(const std::vector<uint8_t> &data) {
    this->data_static_ = data;
    this->static_ = true;
  }

  void set_can_id(uint32_t can_id) { this->can_id_ = can_id; }

  void set_can_ext_id(bool can_ext_id) { this->can_ext_id_ = can_ext_id; }

  void play(Ts... x) override {
    auto can_id = this->can_id_.has_value() ? *this->can_id_ : this->parent_->can_id_;
    auto can_ext_id = this->can_ext_id_.has_value() ? *this->can_ext_id_ : this->parent_->can_ext_id_;
    if (this->static_) {
      this->parent_->send_data(can_id, can_ext_id, this->data_static_);
    } else {
      auto val = this->data_func_(x...);
      this->parent_->send_data(can_id, can_ext_id, val);
    }
  }

 protected:
  optional<uint32_t> can_id_{};
  optional<bool> can_ext_id_{};
  bool static_{false};
  std::function<std::vector<uint8_t>(Ts...)> data_func_{};
  std::vector<uint8_t> data_static_{};
};

class CanbusTrigger : public Trigger<std::vector<uint8_t>>, public Component {
  friend class Canbus;

 public:
  explicit CanbusTrigger(Canbus *parent, const std::uint32_t can_id, const bool can_ext_id)
      : parent_(parent), can_id_(can_id), can_ext_id_(can_ext_id){};
  void setup() override { this->parent_->add_trigger(this); }

 protected:
  Canbus *parent_;
  uint32_t can_id_;
  bool can_ext_id_;
};

}  // namespace canbus
}  // namespace esphome
