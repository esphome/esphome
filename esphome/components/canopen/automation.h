#pragma once

#include "canopen.h"
#include "esphome/core/automation.h"

namespace esphome::canopen {

template<typename... Ts> class SDOWriteAction : public Action<Ts...> {
 public:
  SDOWriteAction(CANopenComponent *parent, uint8_t node, uint16_t index, uint8_t subindex, uint8_t size)
      : parent_(parent), index_(index), node_(node), subindex_(subindex), size_(size) {}
  TEMPLATABLE_VALUE(uint32_t, value)

  template<typename F> void add_on_success_callback(F &&callback) {
    this->success_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_error_callback(F &&callback) {
    this->error_callback_.add(std::forward<F>(callback));
  }

  void play(const Ts &...x) override {
    if (!this->parent_->write_sdo(this->node_, this->index_, this->subindex_, this->value_.value(x...), this->size_,
                                  SDOWriteAction::on_result_, this)) {
      this->error_callback_.call(SDOResult::SDO_RESULT_REJECTED, 0);
    }
  }

 protected:
  static void on_result_(void *context, SDOResult result, uint32_t value, uint8_t size) {
    auto *action = static_cast<SDOWriteAction *>(context);
    if (result == SDOResult::SDO_RESULT_SUCCESS)
      action->success_callback_.call();
    else
      action->error_callback_.call(result, value);
  }

  CANopenComponent *parent_;
  LazyCallbackManager<void()> success_callback_;
  LazyCallbackManager<void(SDOResult, uint32_t)> error_callback_;
  uint16_t index_;
  uint8_t node_;
  uint8_t subindex_;
  uint8_t size_;
};

}  // namespace esphome::canopen
