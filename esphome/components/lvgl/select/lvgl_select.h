#pragma once

#include <utility>

#include "esphome/components/select/select.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "../lvgl.h"

namespace esphome {
namespace lvgl {

class LVGLSelect : public select::Select, public Component {
 public:
  LVGLSelect(LvSelectable *widget, lv_anim_enable_t anim, bool restore)
      : widget_(widget), anim_(anim), restore_(restore) {}

  void setup() override {
    this->set_options_();
    if (this->restore_) {
      size_t index;
      this->pref_ = global_preferences->make_preference<size_t>(this->get_preference_hash());
      if (this->pref_.load(&index))
        this->widget_->set_selected_index(index, LV_ANIM_OFF);
    }
    this->publish();
    lv_obj_add_event_cb(
        this->widget_->obj,
        [](lv_event_t *e) {
          auto *it = static_cast<LVGLSelect *>(e->user_data);
          it->set_options_();
        },
        LV_EVENT_REFRESH, this);
    auto lamb = [](lv_event_t *e) {
      auto *self = static_cast<LVGLSelect *>(e->user_data);
      self->publish();
    };
    lv_obj_add_event_cb(this->widget_->obj, lamb, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(this->widget_->obj, lamb, lv_update_event, this);
  }

  void publish() {
    auto index = this->widget_->get_selected_index();
    this->publish_state(index);
    if (this->restore_) {
      this->pref_.save(&index);
    }
  }

 protected:
  void control(size_t index) override {
    this->widget_->set_selected_index(index, this->anim_);
    this->publish();
  }
  void set_options_() {
    // Widget uses std::vector<std::string>, SelectTraits uses FixedVector<const char*>
    // Convert by extracting c_str() pointers
    const auto &opts = this->widget_->get_options();
    FixedVector<const char *> opt_ptrs;
    opt_ptrs.init(opts.size());
    for (const auto &opt : opts) {
      opt_ptrs.push_back(opt.c_str());
    }
    this->traits.set_options(opt_ptrs);
  }

  LvSelectable *widget_;
  lv_anim_enable_t anim_;
  bool restore_;
  ESPPreferenceObject pref_{};
};

}  // namespace lvgl
}  // namespace esphome
