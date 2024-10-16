#pragma once

#include <utility>

#include "esphome/components/select/select.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "../lvgl.h"

namespace esphome {
namespace lvgl {

class LVGLSelect : public select::Select {
 public:
  void set_widget(LvSelectable *widget, lv_anim_enable_t anim = LV_ANIM_OFF) {
    this->widget_ = widget;
    this->anim_ = anim;
    this->set_options_();
    lv_obj_add_event_cb(
        this->widget_->obj,
        [](lv_event_t *e) {
          auto *it = static_cast<LVGLSelect *>(e->user_data);
          it->set_options_();
        },
        LV_EVENT_REFRESH, this);
    if (this->initial_state_.has_value()) {
      this->control(this->initial_state_.value());
      this->initial_state_.reset();
    }
    this->publish();
    auto lamb = [](lv_event_t *e) {
      auto *self = static_cast<LVGLSelect *>(e->user_data);
      self->publish();
    };
    lv_obj_add_event_cb(this->widget_->obj, lamb, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(this->widget_->obj, lamb, lv_update_event, this);
  }

  void publish() { this->publish_state(this->widget_->get_selected_text()); }

 protected:
  void control(const std::string &value) override {
    if (this->widget_ != nullptr) {
      this->widget_->set_selected_text(value, this->anim_);
    } else {
      this->initial_state_ = value;
    }
  }
  void set_options_() { this->traits.set_options(this->widget_->get_options()); }

  LvSelectable *widget_{};
  optional<std::string> initial_state_{};
  lv_anim_enable_t anim_{LV_ANIM_OFF};
};

}  // namespace lvgl
}  // namespace esphome
