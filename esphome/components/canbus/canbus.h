#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/optional.h"

#include <cinttypes>
#include <vector>

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
  CAN_1KBPS,
  CAN_5KBPS,
  CAN_10KBPS,
  CAN_12K5BPS,
  CAN_16KBPS,
  CAN_20KBPS,
  CAN_25KBPS,
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
  CAN_800KBPS,
  CAN_1000KBPS
};

class CanbusTrigger;
template<typename... Ts> class CanbusSendAction;

/* CAN payload length definitions according to ISO 11898-1 */
static const uint8_t CAN_MAX_DATA_LENGTH = 8;

/*
Can Frame describes a normative CAN Frame
The RTR = Remote Transmission Request is implemented in every CAN controller but rarely used
So currently the flag is passed to and from the hardware but currently ignored to the user application.
*/
struct CanFrame {
  bool use_extended_id = false;
  bool remote_transmission_request = false;
  uint32_t can_id;              /* 29 or 11 bit CAN_ID  */
  uint8_t can_data_length_code; /* frame payload length in byte (0 .. CAN_MAX_DATA_LENGTH) */
  uint8_t data[CAN_MAX_DATA_LENGTH] __attribute__((aligned(8)));
};

class Canbus : public Component {
 public:
  Canbus(){};
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void loop() override;

  void send_data(uint32_t can_id, bool use_extended_id, bool remote_transmission_request,
                 const std::vector<uint8_t> &data);
  void send_data(uint32_t can_id, bool use_extended_id, const std::vector<uint8_t> &data) {
    // for backwards compatibility only
    this->send_data(can_id, use_extended_id, false, data);
  }
  void set_can_id(uint32_t can_id) { this->can_id_ = can_id; }
  void set_use_extended_id(bool use_extended_id) { this->use_extended_id_ = use_extended_id; }
  void set_bitrate(CanSpeed bit_rate) { this->bit_rate_ = bit_rate; }

  void add_trigger(CanbusTrigger *trigger);
  /**
   * Add a callback to be called when a CAN message is received. All received messages
   * are passed to the callback without filtering.
   *
   * The callback function receives:
   * - can_id of the received data
   * - extended_id True if the can_id is an extended id
   * - rtr If this is a remote transmission request
   * - data The message data
   */
  void add_callback(
      std::function<void(uint32_t can_id, bool extended_id, bool rtr, const std::vector<uint8_t> &data)> callback) {
    this->callback_manager_.add(std::move(callback));
  }

 protected:
  template<typename... Ts> friend class CanbusSendAction;
  std::vector<CanbusTrigger *> triggers_{};
  uint32_t can_id_;
  bool use_extended_id_;
  CanSpeed bit_rate_;
  CallbackManager<void(uint32_t can_id, bool extended_id, bool rtr, const std::vector<uint8_t> &data)>
      callback_manager_{};

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

  void set_use_extended_id(bool use_extended_id) { this->use_extended_id_ = use_extended_id; }

  void set_remote_transmission_request(bool remote_transmission_request) {
    this->remote_transmission_request_ = remote_transmission_request;
  }

  void play(Ts... x) override {
    auto can_id = this->can_id_.has_value() ? *this->can_id_ : this->parent_->can_id_;
    auto use_extended_id =
        this->use_extended_id_.has_value() ? *this->use_extended_id_ : this->parent_->use_extended_id_;
    if (this->static_) {
      this->parent_->send_data(can_id, use_extended_id, this->remote_transmission_request_, this->data_static_);
    } else {
      auto val = this->data_func_(x...);
      this->parent_->send_data(can_id, use_extended_id, this->remote_transmission_request_, val);
    }
  }

 protected:
  optional<uint32_t> can_id_{};
  optional<bool> use_extended_id_{};
  bool remote_transmission_request_{false};
  bool static_{false};
  std::function<std::vector<uint8_t>(Ts...)> data_func_{};
  std::vector<uint8_t> data_static_{};
};

class CanbusTrigger : public Trigger<std::vector<uint8_t>, uint32_t, bool>, public Component {
  friend class Canbus;

 public:
  explicit CanbusTrigger(Canbus *parent, const std::uint32_t can_id, const std::uint32_t can_id_mask,
                         const bool use_extended_id)
      : parent_(parent), can_id_(can_id), can_id_mask_(can_id_mask), use_extended_id_(use_extended_id){};

  void set_remote_transmission_request(bool remote_transmission_request) {
    this->remote_transmission_request_ = remote_transmission_request;
  }

  void setup() override { this->parent_->add_trigger(this); }

 protected:
  Canbus *parent_;
  uint32_t can_id_;
  uint32_t can_id_mask_;
  bool use_extended_id_;
  optional<bool> remote_transmission_request_{};
};

}  // namespace canbus
}  // namespace esphome
