#include "text_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace text_sensor {

static const char *const TAG = "text_sensor";

TextSensor::TextSensor() : TextSensor("") {}
TextSensor::TextSensor(const std::string &name) : EntityBase(name) {}

void TextSensor::publish_state(const std::string &state) {
  this->raw_state = state;
  this->raw_callback_.call(state);

  ESP_LOGV(TAG, "'%s': Received new state %s", this->name_.c_str(), state.c_str());

  if (this->filter_list_ == nullptr) {
    this->internal_send_state_to_frontend(state);
  } else {
    this->filter_list_->input(state);
  }
}

void TextSensor::add_filter(Filter *filter) {
  // inefficient, but only happens once on every sensor setup and nobody's going to have massive amounts of
  // filters
  ESP_LOGVV(TAG, "TextSensor(%p)::add_filter(%p)", this, filter);
  if (this->filter_list_ == nullptr) {
    this->filter_list_ = filter;
  } else {
    Filter *last_filter = this->filter_list_;
    while (last_filter->next_ != nullptr)
      last_filter = last_filter->next_;
    last_filter->initialize(this, filter);
  }
  filter->initialize(this, nullptr);
}
void TextSensor::add_filters(const std::vector<Filter *> &filters) {
  for (Filter *filter : filters) {
    this->add_filter(filter);
  }
}
void TextSensor::set_filters(const std::vector<Filter *> &filters) {
  this->clear_filters();
  this->add_filters(filters);
}
void TextSensor::clear_filters() {
  if (this->filter_list_ != nullptr) {
    ESP_LOGVV(TAG, "TextSensor(%p)::clear_filters()", this);
  }
  this->filter_list_ = nullptr;
}

void TextSensor::add_on_state_callback(std::function<void(std::string)> callback) {
  this->callback_.add(std::move(callback));
}
void TextSensor::add_on_raw_state_callback(std::function<void(std::string)> callback) {
  this->raw_callback_.add(std::move(callback));
}

std::string TextSensor::get_state() const { return this->state; }
std::string TextSensor::get_raw_state() const { return this->raw_state; }
void TextSensor::internal_send_state_to_frontend(const std::string &state) {
  this->state = state;
  this->has_state_ = true;
  ESP_LOGD(TAG, "'%s': Sending state '%s'", this->name_.c_str(), state.c_str());
  this->callback_.call(state);
}

std::string TextSensor::unique_id() { return ""; }
bool TextSensor::has_state() { return this->has_state_; }
uint32_t TextSensor::hash_base() { return 334300109UL; }

}  // namespace text_sensor
}  // namespace esphome
