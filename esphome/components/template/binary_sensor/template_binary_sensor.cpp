#include "template_binary_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace template_ {

static const char *const TAG = "template.binary_sensor";

#ifdef USE_TEMPLATE_BINARY_SENSOR_DYNAMIC_LAMBDA
TemplateBinarySensor::~TemplateBinarySensor() { this->destroy_runtime_(); }
#endif

void TemplateBinarySensor::setup() {
  if (!this->publish_initial_state_)
    return;

  if (this->f_ != nullptr) {
    this->publish_initial_state(this->f_().value_or(false));
  } else {
    this->publish_initial_state(false);
  }
}
void TemplateBinarySensor::loop() {
#ifdef USE_TEMPLATE_BINARY_SENSOR_DYNAMIC_LAMBDA
  if (this->dynamic_)
    return;
#endif
  if (this->f_ == nullptr)
    return;

  auto s = this->f_();
  if (s.has_value()) {
    this->publish_state(*s);
  }
}
void TemplateBinarySensor::dump_config() {
  LOG_BINARY_SENSOR("", "Template Binary Sensor", this);
#ifdef USE_TEMPLATE_BINARY_SENSOR_DYNAMIC_LAMBDA
  ESP_LOGCONFIG(TAG, "  Dynamic lambda: %s", YESNO(this->dynamic_));
#endif
}

#ifdef USE_TEMPLATE_BINARY_SENSOR_DYNAMIC_LAMBDA
void TemplateBinarySensor::set_lambda_source(text_sensor::TextSensor *source) {
  this->lambda_source_ = source;
  if (source == nullptr)
    return;

  source->add_on_state_callback([this](std::string value) { this->handle_source_update_(value); });
  if (source->has_state())
    this->handle_source_update_(source->state);
}

void TemplateBinarySensor::handle_source_update_(const std::string &source) {
  if (!this->dynamic_)
    return;

  if (source == this->last_source_)
    return;

  this->last_source_ = source;
  this->execute_wrench_(source);
}

bool TemplateBinarySensor::ensure_runtime_() {
  if (this->wrench_state_ != nullptr)
    return true;

  this->wrench_state_ = wr_newState();
  if (this->wrench_state_ == nullptr) {
    ESP_LOGE(TAG, "Failed to initialize Wrench state");
    return false;
  }

  return true;
}

void TemplateBinarySensor::destroy_runtime_() {
  if (this->wrench_state_ == nullptr)
    return;

  wr_destroyState(this->wrench_state_);
  this->wrench_state_ = nullptr;
}

void TemplateBinarySensor::execute_wrench_(const std::string &source) {
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
