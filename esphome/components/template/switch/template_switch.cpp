#include "template_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace template_ {

static const char *const TAG = "template.switch";

TemplateSwitch::TemplateSwitch() : turn_on_trigger_(new Trigger<>()), turn_off_trigger_(new Trigger<>()) {}

void TemplateSwitch::loop() {
#ifdef USE_TEMPLATE_SWITCH_DYNAMIC_LAMBDA
  if (this->dynamic_)
    return;
#endif
  if (!this->f_.has_value())
    return;
  auto s = (*this->f_)();
  if (!s.has_value())
    return;

  this->publish_state(*s);
}
void TemplateSwitch::write_state(bool state) {
  if (this->prev_trigger_ != nullptr) {
    this->prev_trigger_->stop_action();
  }

  if (state) {
    this->prev_trigger_ = this->turn_on_trigger_;
    this->turn_on_trigger_->trigger();
  } else {
    this->prev_trigger_ = this->turn_off_trigger_;
    this->turn_off_trigger_->trigger();
  }

  if (this->optimistic_)
    this->publish_state(state);
}
void TemplateSwitch::set_optimistic(bool optimistic) { this->optimistic_ = optimistic; }
bool TemplateSwitch::assumed_state() { return this->assumed_state_; }
void TemplateSwitch::set_state_lambda(std::function<optional<bool>()> &&f) { this->f_ = f; }
float TemplateSwitch::get_setup_priority() const { return setup_priority::HARDWARE - 2.0f; }
Trigger<> *TemplateSwitch::get_turn_on_trigger() const { return this->turn_on_trigger_; }
Trigger<> *TemplateSwitch::get_turn_off_trigger() const { return this->turn_off_trigger_; }
void TemplateSwitch::setup() {
  optional<bool> initial_state = this->get_initial_state_with_restore_mode();

  if (initial_state.has_value()) {
    ESP_LOGD(TAG, "  Restored state %s", ONOFF(initial_state.value()));
    // if it has a value, restore_mode is not "DISABLED", therefore act on the switch:
    if (initial_state.value()) {
      this->turn_on();
    } else {
      this->turn_off();
    }
  }
}
void TemplateSwitch::dump_config() {
  LOG_SWITCH("", "Template Switch", this);
  ESP_LOGCONFIG(TAG, "  Optimistic: %s", YESNO(this->optimistic_));
#ifdef USE_TEMPLATE_SWITCH_DYNAMIC_LAMBDA
  ESP_LOGCONFIG(TAG, "  Dynamic lambda: %s", YESNO(this->dynamic_));
#endif
}
void TemplateSwitch::set_assumed_state(bool assumed_state) { this->assumed_state_ = assumed_state; }

#ifdef USE_TEMPLATE_SWITCH_DYNAMIC_LAMBDA
TemplateSwitch::~TemplateSwitch() { this->destroy_runtime_(); }

void TemplateSwitch::set_lambda_source(text_sensor::TextSensor *source) {
  this->lambda_source_ = source;
  if (source == nullptr)
    return;

  source->add_on_state_callback([this](std::string value) { this->handle_source_update_(value); });
  if (source->has_state())
    this->handle_source_update_(source->state);
}

void TemplateSwitch::handle_source_update_(const std::string &source) {
  if (!this->dynamic_)
    return;

  if (source == this->last_source_)
    return;

  this->last_source_ = source;
  this->execute_wrench_(source);
}

bool TemplateSwitch::ensure_runtime_() {
  if (this->wrench_state_ != nullptr)
    return true;

  this->wrench_state_ = wr_newState();
  if (this->wrench_state_ == nullptr) {
    ESP_LOGE(TAG, "Failed to initialize Wrench state");
    return false;
  }

  return true;
}

void TemplateSwitch::destroy_runtime_() {
  if (this->wrench_state_ == nullptr)
    return;

  wr_destroyState(this->wrench_state_);
  this->wrench_state_ = nullptr;
}

void TemplateSwitch::execute_wrench_(const std::string &source) {
  if (!this->ensure_runtime_())
    return;

  if (source.empty()) {
    ESP_LOGW(TAG, "Dynamic lambda script empty");
    return;
  }

  unsigned char *bytecode = nullptr;
  int bytecode_length = 0;
  char error_message[128] = {0};
  WRError compile_error = wr_compile(source.c_str(), static_cast<int>(source.size()), &bytecode, &bytecode_length, error_message);
  if (compile_error != WR_ERR_None) {
    ESP_LOGE(TAG, "Failed to compile dynamic lambda (%d): %s", static_cast<int>(compile_error), error_message);
    if (bytecode != nullptr)
      wr_free(bytecode);
    return;
  }

  WRContext *context = wr_run(this->wrench_state_, bytecode, bytecode_length, false, false);
  if (context == nullptr) {
    WRError runtime_error = wr_getLastError(this->wrench_state_);
    ESP_LOGE(TAG, "Dynamic lambda execution failed (%d)", static_cast<int>(runtime_error));
    wr_free(bytecode);
    return;
  }

  WRValue *result_value = wr_getGlobalRef(context, "result");
  if (result_value == nullptr)
    result_value = wr_returnValueFromLastCall(context);

  bool recognized = false;
  bool payload = false;

  if (result_value != nullptr) {
    if (result_value->isBool()) {
      payload = result_value->asBool();
      recognized = true;
    } else if (result_value->isInt()) {
      payload = result_value->asInt() != 0;
      recognized = true;
    } else if (result_value->isFloat()) {
      payload = result_value->asFloat() != 0.0f;
      recognized = true;
    } else {
      ESP_LOGW(TAG, "Unsupported dynamic lambda result type");
    }
  } else {
    ESP_LOGW(TAG, "Dynamic lambda did not produce a result");
  }

  wr_destroyContext(context);
  wr_free(bytecode);

  if (!recognized)
    return;

  this->publish_state(payload);
}
#endif

}  // namespace template_
}  // namespace esphome
