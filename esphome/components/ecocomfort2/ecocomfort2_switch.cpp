#ifdef USE_ESP32

#include "ecocomfort2_switch.h"
#include "esphome/core/log.h"

#include <cstring>

namespace esphome {
namespace ecocomfort2 {

static const char *const TAG = "ecocomfort2.switch";

void Ecocomfort2AdvancedSwitch::dump_config() {
  ESP_LOGCONFIG(TAG, "Ecocomfort2 Advanced Switch (%s):", this->advanced_type_ != nullptr ? this->advanced_type_ : "?");
  LOG_SWITCH("  ", "Advanced", this);
}

void Ecocomfort2AdvancedSwitch::write_state(bool state) {
  if (!this->parent_->is_ready()) {
    ESP_LOGW(TAG, "Not ready, cannot change advanced mode");
    return;
  }
  if (!this->parent_->has_config_data()) {
    ESP_LOGW(TAG, "Configuration not loaded yet, cannot change advanced mode");
    return;
  }

  // Update hub's internal advanced flag, then write thresholds
  // write_thresholds() reads advanced flags from the hub's internal state
  if (this->advanced_type_ != nullptr && std::strcmp(this->advanced_type_, "humidity") == 0) {
    this->parent_->set_humidity_advanced(state);
  } else if (this->advanced_type_ != nullptr && std::strcmp(this->advanced_type_, "voc") == 0) {
    this->parent_->set_voc_advanced(state);
  }

  this->parent_->write_thresholds(this->parent_->get_humidity_threshold(), this->parent_->get_luminosity_threshold(),
                                  this->parent_->get_voc_threshold());

  this->publish_state(state);
}

void Ecocomfort2AdvancedSwitch::on_config() {
  if (!this->parent_->has_config_data()) {
    return;
  }

  bool state;
  if (this->advanced_type_ != nullptr && std::strcmp(this->advanced_type_, "humidity") == 0) {
    state = this->parent_->get_humidity_advanced();
  } else if (this->advanced_type_ != nullptr && std::strcmp(this->advanced_type_, "voc") == 0) {
    state = this->parent_->get_voc_advanced();
  } else {
    return;
  }

  if (!this->has_state() || this->state != state) {
    this->publish_state(state);
  }
}

}  // namespace ecocomfort2
}  // namespace esphome

#endif
