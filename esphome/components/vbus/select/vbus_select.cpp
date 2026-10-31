#include "vbus_select.h"

#ifdef USE_VBUS_CS_PLUS_ENTITIES
#include "esphome/core/controller_registry.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome::vbus {

ESPHOME_LOG_TAG(TAG, "vbus.select");

void VBusSettingSelect::initialize_options() {
  const auto &labels = this->traits.get_options();
  this->labels_ = ConstVector<const char *>(labels.data(), labels.size());
  this->options_.init(labels.size());
  this->option_mask_ = ((1u << labels.size()) - 1) << this->minimum_;
}

void VBusSettingSelect::dump_setting_config() {
  LOG_SELECT("", "VBus Setting", this);
  ESP_LOGCONFIG(TAG, "  Setting ID: %u", this->index_);
}

void VBusSettingSelect::on_setting_snapshot() {
  if (!this->controller_->has_fresh_snapshot() && this->controller_->has_recent_snapshot())
    return;
  const auto previous = this->current_option();
  const uint32_t mask = this->controller_->setting_option_mask(this->index_);
  if (mask && mask != this->option_mask_) {
    this->options_.clear();
    for (size_t i = 0; i < this->labels_.size(); ++i) {
      if (mask & (1u << (this->minimum_ + i)))
        this->options_.push_back(this->labels_[i]);
    }
    // Traits borrow this fixed buffer; runtime updates never allocate or copy an owned list.
    this->traits.set_options_static(this->options_.begin(), this->options_.size());
    this->option_mask_ = mask;
    this->controller_->note_metadata_change();
  }
  const float value = mask ? this->controller_->setting_value(this->index_) : NAN;
  if (std::isfinite(value)) {
    const int32_t position = static_cast<int32_t>(value) - this->minimum_;
    if (position >= 0 && static_cast<size_t>(position) < this->labels_.size()) {
      const char *label = this->labels_[position];
      const auto active = this->index_of(label);
      if (active.has_value()) {
        if (!this->has_state() || previous != label) {
          this->publish_state(*active);
        } else {
          this->active_index_ = *active;
        }
        return;
      }
    }
  }
  if (this->has_state()) {
    this->set_has_state(false);
#ifdef USE_CONTROLLER_REGISTRY
    ControllerRegistry::notify_select_update(this);
#endif
  }
}

void VBusSettingSelect::control(size_t index) {
  if (index >= this->size())
    return;
  const auto label = StringRef(this->option_at(index));
  for (size_t i = 0; i < this->labels_.size(); ++i) {
    if (label == this->labels_[i]) {
      if (!this->controller_->request_setting(this->index_, this->minimum_ + i)) {
        ESP_LOGW(TAG, "Setting %u request rejected; retaining confirmed state", this->index_);
      }
      return;
    }
  }
}

}  // namespace esphome::vbus
#endif
