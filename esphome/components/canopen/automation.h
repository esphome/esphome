#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/defines.h"

#include "canopen.h"

namespace esphome::canopen {

// canopen.sdo_write: fires the request and continues; on_success / on_error run when it ends
template<typename... Ts> class SDOWriteAction : public Action<Ts...>, public Parented<CANopenComponent> {
 public:
  TEMPLATABLE_VALUE(uint8_t, node_id)
  TEMPLATABLE_VALUE(uint16_t, index)
  TEMPLATABLE_VALUE(uint8_t, subindex)
  TEMPLATABLE_VALUE(uint32_t, value)
  void set_size(uint8_t size) { this->size_ = size; }
  Trigger<> *get_success_trigger() { return &this->success_trigger_; }
  Trigger<SDOResult, uint32_t> *get_error_trigger() { return &this->error_trigger_; }

  void play(const Ts &...x) override {
    this->parent_->write_sdo(this->node_id_.value(x...), this->index_.value(x...), this->subindex_.value(x...),
                             this->value_.value(x...), this->size_, on_reply, this);
  }

 protected:
  static void on_reply(void *context, const SDOReply &reply) {
    auto *self = static_cast<SDOWriteAction *>(context);
    if (reply.ok()) {
      self->success_trigger_.trigger();
    } else {
      self->error_trigger_.trigger(reply.result, reply.abort_code);
    }
  }

  uint8_t size_{4};
  Trigger<> success_trigger_;
  Trigger<SDOResult, uint32_t> error_trigger_;
};

// canopen.sdo_read: fires the request and continues; on_value / on_error run when it ends
template<typename... Ts> class SDOReadAction : public Action<Ts...>, public Parented<CANopenComponent> {
 public:
  TEMPLATABLE_VALUE(uint8_t, node_id)
  TEMPLATABLE_VALUE(uint16_t, index)
  TEMPLATABLE_VALUE(uint8_t, subindex)
  // 1/2/4 checks the width (SIZE_MISMATCH), 0 accepts any width up to 4
  void set_size(uint8_t size) { this->size_ = size; }
  Trigger<uint32_t> *get_value_trigger() { return &this->value_trigger_; }
  Trigger<SDOResult, uint32_t> *get_error_trigger() { return &this->error_trigger_; }

  void play(const Ts &...x) override {
    this->parent_->read_sdo(this->node_id_.value(x...), this->index_.value(x...), this->subindex_.value(x...),
                            this->size_, on_reply, this);
  }

 protected:
  static void on_reply(void *context, const SDOReply &reply) {
    auto *self = static_cast<SDOReadAction *>(context);
    if (reply.ok()) {
      self->value_trigger_.trigger(reply.value);
    } else {
      self->error_trigger_.trigger(reply.result, reply.abort_code);
    }
  }

  uint8_t size_{0};
  Trigger<uint32_t> value_trigger_;
  Trigger<SDOResult, uint32_t> error_trigger_;
};

}  // namespace esphome::canopen
