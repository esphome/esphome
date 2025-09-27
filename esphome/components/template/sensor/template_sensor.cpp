#include "template_sensor.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome {
namespace template_ {

static const char *const TAG = "template.sensor";

#ifdef USE_TEMPLATE_SENSOR_DYNAMIC_LAMBDA
TemplateSensor::~TemplateSensor() { this->destroy_runtime_(); }
#endif

void TemplateSensor::update() {
#ifdef USE_TEMPLATE_SENSOR_DYNAMIC_LAMBDA
  if (this->dynamic_) {
    if (this->lambda_source_ != nullptr) {
      this->handle_source_update_(this->lambda_source_->state);
    }
    return;
  }
#endif
  if (!this->f_.has_value())
    return;

  auto val = (*this->f_)();
  if (val.has_value()) {
    this->publish_state(*val);
  }
}
float TemplateSensor::get_setup_priority() const { return setup_priority::HARDWARE; }
void TemplateSensor::set_template(std::function<optional<float>()> &&f) { this->f_ = f; }
void TemplateSensor::dump_config() {
  LOG_SENSOR("", "Template Sensor", this);
#ifdef USE_TEMPLATE_SENSOR_DYNAMIC_LAMBDA
  ESP_LOGCONFIG(TAG, "  Dynamic lambda: %s", YESNO(this->dynamic_));
#endif
  LOG_UPDATE_INTERVAL(this);
}

#ifdef USE_TEMPLATE_SENSOR_DYNAMIC_LAMBDA
void TemplateSensor::set_lambda_source(text_sensor::TextSensor *source) {
  this->lambda_source_ = source;
  if (source == nullptr)
    return;

  source->add_on_state_callback([this](std::string value) { this->handle_source_update_(value); });
  if (source->has_state())
    this->handle_source_update_(source->state);
}

void TemplateSensor::handle_source_update_(const std::string &source) {
  if (!this->dynamic_)
    return;

  if (source == this->last_source_)
    return;

  this->last_source_ = source;
  this->execute_wrench_(source);
}

bool TemplateSensor::ensure_runtime_() {
  if (this->wrench_state_ != nullptr)
    return true;

  this->wrench_state_ = wr_newState();
  if (this->wrench_state_ == nullptr) {
    ESP_LOGE(TAG, "Failed to initialize Wrench state");
    return false;
  }

  return true;
}

void TemplateSensor::destroy_runtime_() {
  if (this->wrench_state_ == nullptr)
    return;

  wr_destroyState(this->wrench_state_);
  this->wrench_state_ = nullptr;
}

void TemplateSensor::execute_wrench_(const std::string &source) {
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
