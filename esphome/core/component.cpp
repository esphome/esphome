#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/esphal.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {

static const char *TAG = "component";

namespace setup_priority {

const float BUS = 1000.0f;
const float IO = 900.0f;
const float HARDWARE = 800.0f;
const float DATA = 600.0f;
const float PROCESSOR = 400.0;
const float WIFI = 250.0f;
const float AFTER_WIFI = 200.0f;
const float AFTER_CONNECTION = 100.0f;
const float LATE = -100.0f;

}  // namespace setup_priority

const uint32_t COMPONENT_STATE_MASK = 0xFF;
const uint32_t COMPONENT_STATE_CONSTRUCTION = 0x00;
const uint32_t COMPONENT_STATE_SETUP = 0x01;
const uint32_t COMPONENT_STATE_LOOP = 0x02;
const uint32_t COMPONENT_STATE_FAILED = 0x03;
const uint32_t STATUS_LED_MASK = 0xFF00;
const uint32_t STATUS_LED_OK = 0x0000;
const uint32_t STATUS_LED_WARNING = 0x0100;
const uint32_t STATUS_LED_ERROR = 0x0200;

uint32_t global_state = 0;

float Component::get_loop_priority() const { return 0.0f; }

float Component::get_setup_priority() const { return setup_priority::DATA; }

void Component::setup() {}

void Component::loop() {}

void Component::set_interval(const std::string &name, uint32_t interval, std::function<void()> &&f) {  // NOLINT
  App.scheduler.set_interval(this, name, interval, std::move(f));
}

bool Component::cancel_interval(const std::string &name) {  // NOLINT
  return App.scheduler.cancel_interval(this, name);
}

void Component::set_timeout(const std::string &name, uint32_t timeout, std::function<void()> &&f) {  // NOLINT
  return App.scheduler.set_timeout(this, name, timeout, std::move(f));
}

bool Component::cancel_timeout(const std::string &name) {  // NOLINT
  return App.scheduler.cancel_timeout(this, name);
}

void Component::call_loop() { this->loop(); }

void Component::call_setup() { this->setup(); }
uint32_t Component::get_component_state() const { return this->component_state_; }
void Component::call() {
  uint32_t state = this->component_state_ & COMPONENT_STATE_MASK;
  switch (state) {
    case COMPONENT_STATE_CONSTRUCTION:
      // State Construction: Call setup and set state to setup
      this->component_state_ &= ~COMPONENT_STATE_MASK;
      this->component_state_ |= COMPONENT_STATE_SETUP;
      this->call_setup();
      break;
    case COMPONENT_STATE_SETUP:
      // State setup: Call first loop and set state to loop
      this->component_state_ &= ~COMPONENT_STATE_MASK;
      this->component_state_ |= COMPONENT_STATE_LOOP;
      this->call_loop();
      break;
    case COMPONENT_STATE_LOOP:
      // State loop: Call loop
      this->call_loop();
      break;
    case COMPONENT_STATE_FAILED:
      // State failed: Do nothing
      break;
    default:
      break;
  }
}
void Component::mark_failed() {
  ESP_LOGE(TAG, "Component was marked as failed.");
  this->component_state_ &= ~COMPONENT_STATE_MASK;
  this->component_state_ |= COMPONENT_STATE_FAILED;
  this->status_set_error();
}
void Component::defer(std::function<void()> &&f) {  // NOLINT
  App.scheduler.set_timeout(this, "", 0, std::move(f));
}
bool Component::cancel_defer(const std::string &name) {  // NOLINT
  return App.scheduler.cancel_timeout(this, name);
}
void Component::defer(const std::string &name, std::function<void()> &&f) {  // NOLINT
  App.scheduler.set_timeout(this, name, 0, std::move(f));
}
void Component::set_timeout(uint32_t timeout, std::function<void()> &&f) {  // NOLINT
  App.scheduler.set_timeout(this, "", timeout, std::move(f));
}
void Component::set_interval(uint32_t interval, std::function<void()> &&f) {  // NOLINT
  App.scheduler.set_interval(this, "", interval, std::move(f));
}
bool Component::is_failed() { return (this->component_state_ & COMPONENT_STATE_MASK) == COMPONENT_STATE_FAILED; }
bool Component::can_proceed() { return true; }
bool Component::status_has_warning() { return this->component_state_ & STATUS_LED_WARNING; }
bool Component::status_has_error() { return this->component_state_ & STATUS_LED_ERROR; }
void Component::status_set_warning() {
  this->component_state_ |= STATUS_LED_WARNING;
  App.app_state_ |= STATUS_LED_WARNING;
}
void Component::status_set_error() {
  this->component_state_ |= STATUS_LED_ERROR;
  App.app_state_ |= STATUS_LED_ERROR;
}
void Component::status_clear_warning() { this->component_state_ &= ~STATUS_LED_WARNING; }
void Component::status_clear_error() { this->component_state_ &= ~STATUS_LED_ERROR; }
void Component::status_momentary_warning(const std::string &name, uint32_t length) {
  this->status_set_warning();
  this->set_timeout(name, length, [this]() { this->status_clear_warning(); });
}
void Component::status_momentary_error(const std::string &name, uint32_t length) {
  this->status_set_error();
  this->set_timeout(name, length, [this]() { this->status_clear_error(); });
}
void Component::dump_config() {}
float Component::get_actual_setup_priority() const {
  if (isnan(this->setup_priority_override_))
    return this->get_setup_priority();
  return this->setup_priority_override_;
}
void Component::set_setup_priority(float priority) { this->setup_priority_override_ = priority; }

PollingComponent::PollingComponent(uint32_t update_interval) : Component(), update_interval_(update_interval) {}

void PollingComponent::call_setup() {
  // Let the polling component subclass setup their HW.
  this->setup();

  // Register interval.
  this->set_interval("update", this->get_update_interval(), [this]() { this->update(); });
}

uint32_t PollingComponent::get_update_interval() const { return this->update_interval_; }
void PollingComponent::set_update_interval(uint32_t update_interval) { this->update_interval_ = update_interval; }

const std::string &Nameable::get_name() const { return this->name_; }
void Nameable::set_name(const std::string &name) {
  this->name_ = name;
  this->calc_object_id_();
}
Nameable::Nameable(const std::string &name) : name_(name) { this->calc_object_id_(); }

const std::string &Nameable::get_object_id() { return this->object_id_; }
bool Nameable::is_internal() const { return this->internal_; }
void Nameable::set_internal(bool internal) { this->internal_ = internal; }
void Nameable::calc_object_id_() {
  this->object_id_ = sanitize_string_whitelist(to_lowercase_underscore(this->name_), HOSTNAME_CHARACTER_WHITELIST);
  // FNV-1 hash
  this->object_id_hash_ = fnv1_hash(this->object_id_);
}
uint32_t Nameable::get_object_id_hash() { return this->object_id_hash_; }

}  // namespace esphome
