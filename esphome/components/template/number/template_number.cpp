#include "template_number.h"
#include "esphome/core/log.h"

namespace esphome {
namespace template_ {

static const char *const TAG = "template.number";

#ifdef USE_TEMPLATE_NUMBER_DYNAMIC_LAMBDA
TemplateNumber::~TemplateNumber() { this->destroy_runtime_(); }
#endif

void TemplateNumber::setup() {
  if (this->f_.has_value())
    return;

  float value;
  if (!this->restore_value_) {
    value = this->initial_value_;
  } else {
    this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
    if (!this->pref_.load(&value)) {
      if (!std::isnan(this->initial_value_)) {
        value = this->initial_value_;
      } else {
        value = this->traits.get_min_value();
      }
    }
  }
  this->publish_state(value);
}

void TemplateNumber::update() {
#ifdef USE_TEMPLATE_NUMBER_DYNAMIC_LAMBDA
  if (this->dynamic_) {
    if (this->lambda_source_ != nullptr)
      this->handle_source_update_(this->lambda_source_->state);
    return;
  }
#endif
  if (!this->f_.has_value())
    return;

  auto val = (*this->f_)();
  if (!val.has_value())
    return;

  this->publish_state(*val);
}

void TemplateNumber::control(float value) {
  this->set_trigger_->trigger(value);

  if (this->optimistic_)
    this->publish_state(value);

  if (this->restore_value_)
    this->pref_.save(&value);
}
void TemplateNumber::dump_config() {
  LOG_NUMBER("", "Template Number", this);
  ESP_LOGCONFIG(TAG, "  Optimistic: %s", YESNO(this->optimistic_));
#ifdef USE_TEMPLATE_NUMBER_DYNAMIC_LAMBDA
  ESP_LOGCONFIG(TAG, "  Dynamic lambda: %s", YESNO(this->dynamic_));
#endif
  LOG_UPDATE_INTERVAL(this);
}

#ifdef USE_TEMPLATE_NUMBER_DYNAMIC_LAMBDA
void TemplateNumber::set_lambda_source(text_sensor::TextSensor *source) {
  this->lambda_source_ = source;
  if (source == nullptr)
    return;

  source->add_on_state_callback([this](std::string value) { this->handle_source_update_(value); });
  if (source->has_state())
    this->handle_source_update_(source->state);
}

void TemplateNumber::handle_source_update_(const std::string &source) {
  if (!this->dynamic_)
    return;

  if (source == this->last_source_)
    return;

  this->last_source_ = source;
  this->execute_wrench_(source);
}

bool TemplateNumber::ensure_runtime_() {
  if (this->wrench_state_ != nullptr)
    return true;

  this->wrench_state_ = wr_newState();
  if (this->wrench_state_ == nullptr) {
    ESP_LOGE(TAG, "Failed to initialize Wrench state");
    return false;
  }

  return true;
}

void TemplateNumber::destroy_runtime_() {
  if (this->wrench_state_ == nullptr)
    return;

  wr_destroyState(this->wrench_state_);
  this->wrench_state_ = nullptr;
}

void TemplateNumber::execute_wrench_(const std::string &source) {
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
  float payload = NAN;

  if (result_value != nullptr) {
    if (result_value->isFloat()) {
      payload = result_value->asFloat();
      recognized = true;
    } else if (result_value->isInt()) {
      payload = static_cast<float>(result_value->asInt());
      recognized = true;
    } else if (result_value->isBool()) {
      payload = result_value->asBool() ? 1.0f : 0.0f;
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
