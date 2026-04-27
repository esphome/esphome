#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "cover.h"

namespace esphome::cover {

template<typename... Ts> class OpenAction : public Action<Ts...> {
 public:
  explicit OpenAction(Cover *cover) : cover_(cover) {}

  void play(const Ts &...x) override { this->cover_->make_call().set_command_open().perform(); }

 protected:
  Cover *cover_;
};

template<typename... Ts> class CloseAction : public Action<Ts...> {
 public:
  explicit CloseAction(Cover *cover) : cover_(cover) {}

  void play(const Ts &...x) override { this->cover_->make_call().set_command_close().perform(); }

 protected:
  Cover *cover_;
};

template<typename... Ts> class StopAction : public Action<Ts...> {
 public:
  explicit StopAction(Cover *cover) : cover_(cover) {}

  void play(const Ts &...x) override { this->cover_->make_call().set_command_stop().perform(); }

 protected:
  Cover *cover_;
};

template<typename... Ts> class ToggleAction : public Action<Ts...> {
 public:
  explicit ToggleAction(Cover *cover) : cover_(cover) {}

  void play(const Ts &...x) override { this->cover_->make_call().set_command_toggle().perform(); }

 protected:
  Cover *cover_;
};

// Unique Empty<Tag> per field so [[no_unique_address]] is guaranteed to coalesce.
namespace cover_action_detail {
template<int Tag> struct Empty {};
}  // namespace cover_action_detail

// X-macro: (type, field_name, bit_index). Order/bits must match the
// inline field-mask computation in cover_control_to_code in __init__.py:
// stop=bit 0, position=bit 1 (also set by CONF_STATE), tilt=bit 2.
#define COVER_CONTROL_FIELDS(X) \
  X(bool, stop, 0) \
  X(float, position, 1) \
  X(float, tilt, 2)

template<uint16_t Fields, typename... Ts> class ControlAction : public Action<Ts...> {
 public:
  explicit ControlAction(Cover *cover) : cover_(cover) {}

#define COVER_FIELD_SETTER_(type, name, idx) \
  template<typename V> void set_##name(V value) requires((Fields & (1 << (idx))) != 0) { this->name##_ = value; }
#define COVER_FIELD_APPLY_(type, name, idx) \
  if constexpr ((Fields & (1 << (idx))) != 0) \
    call.set_##name(this->name##_.value(x...));
#define COVER_FIELD_DECL_(type, name, idx) \
  [[no_unique_address]] std::conditional_t<(Fields & (1 << (idx))) != 0, TemplatableFn<type, Ts...>, \
                                           cover_action_detail::Empty<(idx)>> \
      name##_{};

  COVER_CONTROL_FIELDS(COVER_FIELD_SETTER_)

  void play(const Ts &...x) override {
    auto call = this->cover_->make_call();
    COVER_CONTROL_FIELDS(COVER_FIELD_APPLY_)
    call.perform();
  }

 protected:
  Cover *cover_;
  COVER_CONTROL_FIELDS(COVER_FIELD_DECL_)
};
#undef COVER_CONTROL_FIELDS

// X-macro: (type, field_name, bit_index). Order/bits must match the
// inline bitmask built in cover_template_publish_to_code in
// template/cover/__init__.py: position=bit 0 (also set by CONF_STATE),
// tilt=bit 1, current_operation=bit 2.
#define COVER_PUBLISH_FIELDS(X) \
  X(float, position, 0) \
  X(float, tilt, 1) \
  X(CoverOperation, current_operation, 2)

template<uint16_t Fields, typename... Ts> class CoverPublishAction : public Action<Ts...> {
 public:
  CoverPublishAction(Cover *cover) : cover_(cover) {}

#define COVER_PUBLISH_SETTER_(type, name, idx) \
  template<typename V> void set_##name(V value) requires((Fields & (1 << (idx))) != 0) { this->name##_ = value; }
#define COVER_PUBLISH_APPLY_(type, name, idx) \
  if constexpr ((Fields & (1 << (idx))) != 0) \
    this->cover_->name = this->name##_.value(x...);
#define COVER_PUBLISH_DECL_(type, name, idx) \
  [[no_unique_address]] std::conditional_t<(Fields & (1 << (idx))) != 0, TemplatableFn<type, Ts...>, \
                                           cover_action_detail::Empty<(idx) + 8>> \
      name##_{};

  COVER_PUBLISH_FIELDS(COVER_PUBLISH_SETTER_)

  void play(const Ts &...x) override {
    COVER_PUBLISH_FIELDS(COVER_PUBLISH_APPLY_)
    this->cover_->publish_state();
  }

 protected:
  Cover *cover_;
  COVER_PUBLISH_FIELDS(COVER_PUBLISH_DECL_)

#undef COVER_PUBLISH_DECL_
#undef COVER_PUBLISH_APPLY_
#undef COVER_PUBLISH_SETTER_
#undef COVER_FIELD_DECL_
#undef COVER_FIELD_APPLY_
#undef COVER_FIELD_SETTER_
};
#undef COVER_PUBLISH_FIELDS

template<bool OPEN, typename... Ts> class CoverPositionCondition : public Condition<Ts...> {
 public:
  CoverPositionCondition(Cover *cover) : cover_(cover) {}

  bool check(const Ts &...x) override { return this->cover_->position == (OPEN ? COVER_OPEN : COVER_CLOSED); }

 protected:
  Cover *cover_;
};

template<typename... Ts> using CoverIsOpenCondition = CoverPositionCondition<true, Ts...>;
template<typename... Ts> using CoverIsClosedCondition = CoverPositionCondition<false, Ts...>;

template<bool OPEN> class CoverPositionTrigger : public Trigger<> {
 public:
  CoverPositionTrigger(Cover *a_cover) : cover_(a_cover) {
    a_cover->add_on_state_callback([this]() {
      if (this->cover_->position != this->last_position_) {
        this->last_position_ = this->cover_->position;
        if (this->cover_->position == (OPEN ? COVER_OPEN : COVER_CLOSED))
          this->trigger();
      }
    });
  }

 protected:
  Cover *cover_;
  float last_position_{NAN};
};

using CoverOpenedTrigger = CoverPositionTrigger<true>;
using CoverClosedTrigger = CoverPositionTrigger<false>;

template<CoverOperation OP> class CoverTrigger : public Trigger<> {
 public:
  CoverTrigger(Cover *a_cover) : cover_(a_cover) {
    a_cover->add_on_state_callback([this]() {
      auto current_op = this->cover_->current_operation;
      if (current_op == OP) {
        if (!this->last_operation_.has_value() || this->last_operation_.value() != OP) {
          this->trigger();
        }
      }
      this->last_operation_ = current_op;
    });
  }

 protected:
  Cover *cover_;
  optional<CoverOperation> last_operation_{};
};
}  // namespace esphome::cover
