#include "esphome/core/log.h"
#include "miio_select.h"

namespace esphome {
namespace miio {

static const char *const TAG = "miio.select";

void MiioSelect::setup() {
  this->parent_->register_listener(this->select_id_, MiioPropertyType::ENUM, [this](const MiioPropertyValue &value) {
    auto enum_value = value.value_enum;

    const auto &mappings = this->mappings_;

    auto it = std::find(mappings.cbegin(), mappings.cend(), enum_value);

    if (it == mappings.end()) {
      ESP_LOGW(TAG, "Invalid value %u", enum_value);
      return;
    }

    auto mapping_idx = static_cast<size_t>(std::distance(mappings.cbegin(), it));

    auto maybe_mapped_value = this->at(mapping_idx);

    if (maybe_mapped_value.has_value()) {
      this->publish_state(maybe_mapped_value.value());
    }
  });
}

void MiioSelect::control(const std::string &value) {
  if (this->optimistic_) {
    this->publish_state(value);
  }

  auto idx = this->index_of(value);

  if (!idx.has_value()) {
    ESP_LOGW(TAG, "Invalid value %s", value.c_str());
    return;
  }

  auto mapping = this->mappings_.at(idx.value());

  this->parent_->set_property(this->select_id_, to_string(mapping));
}

void MiioSelect::dump_config() {
  LOG_SELECT("", "MIIO Select", this);
  ESP_LOGCONFIG(TAG, "  Select has cluster ID: %u", std::get<0>(this->select_id_));
  ESP_LOGCONFIG(TAG, "  Select has key ID: %u", std::get<1>(this->select_id_));
  ESP_LOGCONFIG(TAG, "  Options are:");

  const auto &options = this->traits.get_options();
  for (size_t i = 0; i < this->mappings_.size(); i++) {
    ESP_LOGCONFIG(TAG, "    %i: %s", this->mappings_.at(i), options.at(i));
  }
}

}  // namespace miio
}  // namespace esphome
