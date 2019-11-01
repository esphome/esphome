#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

namespace esphome {
namespace remote_base {

class RCSwitchBase {
 public:
  RCSwitchBase() = default;
  RCSwitchBase(uint32_t sync_high, uint32_t sync_low, uint32_t zero_high, uint32_t zero_low, uint32_t one_high,
               uint32_t one_low, bool inverted);

  void one(RemoteTransmitData *dst) const;

  void zero(RemoteTransmitData *dst) const;

  void sync(RemoteTransmitData *dst) const;

  void transmit(RemoteTransmitData *dst, uint64_t code, uint8_t len) const;

  bool expect_one(RemoteReceiveData &src) const;

  bool expect_zero(RemoteReceiveData &src) const;

  bool expect_sync(RemoteReceiveData &src) const;

  bool decode(RemoteReceiveData &src, uint64_t *out_data, uint8_t *out_nbits) const;

  static void simple_code_to_tristate(uint16_t code, uint8_t nbits, uint64_t *out_code);

  static void type_a_code(uint8_t switch_group, uint8_t switch_device, bool state, uint64_t *out_code,
                          uint8_t *out_nbits);

  static void type_b_code(uint8_t address_code, uint8_t channel_code, bool state, uint64_t *out_code,
                          uint8_t *out_nbits);

  static void type_c_code(uint8_t family, uint8_t group, uint8_t device, bool state, uint64_t *out_code,
                          uint8_t *out_nbits);

  static void type_d_code(uint8_t group, uint8_t device, bool state, uint64_t *out_code, uint8_t *out_nbits);

 protected:
  uint32_t sync_high_{};
  uint32_t sync_low_{};
  uint32_t zero_high_{};
  uint32_t zero_low_{};
  uint32_t one_high_{};
  uint32_t one_low_{};
  bool inverted_{};
};

extern RCSwitchBase rc_switch_protocols[9];

uint64_t decode_binary_string(const std::string &data);

uint64_t decode_binary_string_mask(const std::string &data);

template<typename... Ts> class RCSwitchRawAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(RCSwitchBase, protocol);
  TEMPLATABLE_VALUE(std::string, code);

  void encode(RemoteTransmitData *dst, Ts... x) override {
    auto code = this->code_.value(x...);
    uint64_t the_code = decode_binary_string(code);
    uint8_t nbits = code.size();

    auto proto = this->protocol_.value(x...);
    proto.transmit(dst, the_code, nbits);
  }
};

template<typename... Ts> class RCSwitchTypeAAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(RCSwitchBase, protocol);
  TEMPLATABLE_VALUE(std::string, group);
  TEMPLATABLE_VALUE(std::string, device);
  TEMPLATABLE_VALUE(bool, state);

  void encode(RemoteTransmitData *dst, Ts... x) override {
    auto group = this->group_.value(x...);
    auto device = this->device_.value(x...);
    auto state = this->state_.value(x...);
    uint8_t u_group = decode_binary_string(group);
    uint8_t u_device = decode_binary_string(device);

    uint64_t code;
    uint8_t nbits;
    RCSwitchBase::type_a_code(u_group, u_device, state, &code, &nbits);

    auto proto = this->protocol_.value(x...);
    proto.transmit(dst, code, nbits);
  }
};

template<typename... Ts> class RCSwitchTypeBAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(RCSwitchBase, protocol);
  TEMPLATABLE_VALUE(uint8_t, address);
  TEMPLATABLE_VALUE(uint8_t, channel);
  TEMPLATABLE_VALUE(bool, state);

  void encode(RemoteTransmitData *dst, Ts... x) override {
    auto address = this->address_.value(x...);
    auto channel = this->channel_.value(x...);
    auto state = this->state_.value(x...);

    uint64_t code;
    uint8_t nbits;
    RCSwitchBase::type_b_code(address, channel, state, &code, &nbits);

    auto proto = this->protocol_.value(x...);
    proto.transmit(dst, code, nbits);
  }
};

template<typename... Ts> class RCSwitchTypeCAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(RCSwitchBase, protocol);
  TEMPLATABLE_VALUE(std::string, family);
  TEMPLATABLE_VALUE(uint8_t, group);
  TEMPLATABLE_VALUE(uint8_t, device);
  TEMPLATABLE_VALUE(bool, state);

  void encode(RemoteTransmitData *dst, Ts... x) override {
    auto family = this->family_.value(x...);
    auto group = this->group_.value(x...);
    auto device = this->device_.value(x...);
    auto state = this->state_.value(x...);

    auto u_family = static_cast<uint8_t>(tolower(family[0]) - 'a');

    uint64_t code;
    uint8_t nbits;
    RCSwitchBase::type_c_code(u_family, group, device, state, &code, &nbits);

    auto proto = this->protocol_.value(x...);
    proto.transmit(dst, code, nbits);
  }
};
template<typename... Ts> class RCSwitchTypeDAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(RCSwitchBase, protocol);
  TEMPLATABLE_VALUE(std::string, group);
  TEMPLATABLE_VALUE(uint8_t, device);
  TEMPLATABLE_VALUE(bool, state);

  void encode(RemoteTransmitData *dst, Ts... x) override {
    auto group = this->group_.value(x...);
    auto device = this->device_.value(x...);
    auto state = this->state_.value(x...);

    auto u_group = static_cast<uint8_t>(tolower(group[0]) - 'a');

    uint64_t code;
    uint8_t nbits;
    RCSwitchBase::type_d_code(u_group, device, state, &code, &nbits);

    auto proto = this->protocol_.value(x...);
    proto.transmit(dst, code, nbits);
  }
};

class RCSwitchRawReceiver : public RemoteReceiverBinarySensorBase {
 public:
  void set_protocol(const RCSwitchBase &a_protocol) { this->protocol_ = a_protocol; }
  void set_code(uint64_t code) { this->code_ = code; }
  void set_code(const std::string &code) {
    this->code_ = decode_binary_string(code);
    this->mask_ = decode_binary_string_mask(code);
    this->nbits_ = code.size();
  }
  void set_nbits(uint8_t nbits) { this->nbits_ = nbits; }
  void set_type_a(const std::string &group, const std::string &device, bool state) {
    uint8_t u_group = decode_binary_string(group);
    uint8_t u_device = decode_binary_string(device);
    RCSwitchBase::type_a_code(u_group, u_device, state, &this->code_, &this->nbits_);
  }
  void set_type_b(uint8_t address_code, uint8_t channel_code, bool state) {
    RCSwitchBase::type_b_code(address_code, channel_code, state, &this->code_, &this->nbits_);
  }
  void set_type_c(std::string family, uint8_t group, uint8_t device, bool state) {
    auto u_family = static_cast<uint8_t>(tolower(family[0]) - 'a');
    RCSwitchBase::type_c_code(u_family, group, device, state, &this->code_, &this->nbits_);
  }
  void set_type_d(std::string group, uint8_t device, bool state) {
    auto u_group = static_cast<uint8_t>(tolower(group[0]) - 'a');
    RCSwitchBase::type_d_code(u_group, device, state, &this->code_, &this->nbits_);
  }

 protected:
  bool matches(RemoteReceiveData src) override;

  RCSwitchBase protocol_;
  uint64_t code_;
  uint64_t mask_{0xFFFFFFFFFFFFFFFF};
  uint8_t nbits_;
};

class RCSwitchDumper : public RemoteReceiverDumperBase {
 public:
  bool dump(RemoteReceiveData src) override;
};

}  // namespace remote_base
}  // namespace esphome
