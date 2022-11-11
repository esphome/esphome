#include "section_select.h"
#include "esphome/components/jablotron/jablotron_component.h"

namespace esphome::jablotron_section {

constexpr const char ARMED[] = "ARMED";
constexpr const char ARMED_PART[] = "ARMED_PART";
constexpr const char READY[] = "READY";

constexpr const char TAG[] = "jablotron_section";

void SectionSelect::control(std::string const &value) {
  std::string request;
  if (value == ARMED) {
    request = "SET " + this->get_index_string();
  } else if (value == ARMED_PART) {
    request = "SETP " + this->get_index_string();
  } else if (value == READY) {
    request = "UNSET " + this->get_index_string();
  } else {
    ESP_LOGE(TAG, "Invalid section state: '%s'", value.c_str());
    return;
  }
  this->get_parent_jablotron()->queue_request_access_code(std::move(request), this->get_access_code());
}

void SectionSelect::register_parent(jablotron::JablotronComponent &parent) { parent.register_section(this); }

void SectionSelect::set_state(jablotron::StringView value) {
  if (this->last_value_ != value) {
    this->last_value_ = std::string{value};
    this->publish_state(this->last_value_);
  }
}

}  // namespace esphome::jablotron_section
