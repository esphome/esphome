#include "template_text.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <memory>
#include <string>

namespace esphome {
namespace template_ {

namespace {
struct WrenchBytecodeDeleter {
  void operator()(unsigned char *ptr) const {
    if (ptr != nullptr) {
      wr_free(ptr);
    }
  }
};
}  // namespace

static const char *const TAG = "template.text";

#ifdef USE_TEMPLATE_TEXT_DYNAMIC_LAMBDA
TemplateText::~TemplateText() { this->destroy_runtime_(); }
#endif

void TemplateText::setup() {
  if (!(this->f_ == nullptr)) {
    if (this->f_.has_value())
      return;
  }

  std::string value = this->initial_value_;
  if (!this->pref_) {
    ESP_LOGD(TAG, "State from initial: %s", value.c_str());
  } else {
    uint32_t key = this->get_preference_hash();
    key += this->traits.get_min_length() << 2;
    key += this->traits.get_max_length() << 4;
    key += fnv1_hash(this->traits.get_pattern()) << 6;
    this->pref_->setup(key, value);
  }
  if (!value.empty())
    this->publish_state(value);

#ifdef USE_TEMPLATE_TEXT_DYNAMIC_LAMBDA
  if (this->dynamic_ && !value.empty())
    this->execute_wrench_(value);
#endif
}

void TemplateText::update() {
#ifdef USE_TEMPLATE_TEXT_DYNAMIC_LAMBDA
  if (this->dynamic_)
    this->execute_wrench_(this->state);
#endif

  if (this->f_ == nullptr)
    return;

  if (!this->f_.has_value())
    return;

  auto val = (*this->f_)();
  if (!val.has_value())
    return;

  this->publish_state(*val);\
#ifdef USE_TEMPLATE_TEXT_DYNAMIC_LAMBDA
  if (this->dynamic_)
    this->execute_wrench_(*val);
#endif

void TemplateText::execute_wrench(const std::string &value) {
  unsigned char *bytecode_raw = nullptr;
  int bytecode_len = 0;
  int err = wr_compile(value.c_str(), value.size(), &bytecode_raw, &bytecode_len);  // compile it

  ESP_LOGD(TAG, "wrench err: %i", err);
  if (err != 0) {
    if (bytecode_raw != nullptr) {
      wr_free(bytecode_raw);
    }
    return;
  }

  std::unique_ptr<unsigned char, WrenchBytecodeDeleter> bytecode(bytecode_raw);
  WRContext *gc = wr_run(this->w, bytecode.get(), bytecode_len, false);  // load and run the code!

  if (gc == nullptr) {
    return;
  }

  WRValue *g = wr_getGlobalRef(gc, "result");

  if (g != nullptr) {
    char buf[1024];
    this->lambda_result_->publish_state(g->asString(buf, 1024));
  }
}

void TemplateText::control(const std::string &value) {
  this->set_trigger_->trigger(value);

#ifdef USE_TEMPLATE_TEXT_DYNAMIC_LAMBDA
  if (this->dynamic_)
    this->execute_wrench_(value);
#endif

  if (this->optimistic_)
    this->publish_state(value);

  if (this->pref_) {
    if (!this->pref_->save(value)) {
      ESP_LOGW(TAG, "Text value too long to save");
    }
  }
}

void TemplateText::dump_config() {
  LOG_TEXT("", "Template Text Input", this);
  ESP_LOGCONFIG(TAG, "  Optimistic: %s", YESNO(this->optimistic_));
#ifdef USE_TEMPLATE_TEXT_DYNAMIC_LAMBDA
  ESP_LOGCONFIG(TAG, "  Dynamic lambda: %s", YESNO(this->dynamic_));
#endif
  LOG_UPDATE_INTERVAL(this);
}

#ifdef USE_TEMPLATE_TEXT_DYNAMIC_LAMBDA
bool TemplateText::ensure_runtime_() {
  if (this->wrench_state_ != nullptr)
    return true;

  this->wrench_state_ = wr_newState();
  if (this->wrench_state_ == nullptr) {
    ESP_LOGE(TAG, "Failed to initialize Wrench state");
    return false;
  }

  return true;
}

void TemplateText::destroy_runtime_() {
  if (this->wrench_state_ == nullptr)
    return;

  wr_destroyState(this->wrench_state_);
  this->wrench_state_ = nullptr;
}

void TemplateText::execute_wrench_(const std::string &source) {
  if (!this->lambda_result_) {
    ESP_LOGW(TAG, "Dynamic lambda configured without helper text sensor");
    return;
  }

  if (!this->ensure_runtime_())
    return;

  if (source.empty()) {
    this->lambda_result_->publish_state("");
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

  std::string payload;
  bool recognized = false;

  if (result_value != nullptr) {
    if (result_value->isString()) {
      WRValue::MallocStrScoped value_string(*result_value);
      if (value_string) {
        payload.assign(value_string, value_string.size());
        recognized = true;
      }
    } else if (result_value->isInt()) {
      payload = str_sprintf("%d", result_value->asInt());
      recognized = true;
    } else if (result_value->isFloat()) {
      payload = str_sprintf("%.6f", result_value->asFloat());
      auto dot = payload.find('.');
      if (dot != std::string::npos) {
        while (!payload.empty() && payload.back() == '0')
          payload.pop_back();
        if (!payload.empty() && payload.back() == '.')
          payload.pop_back();
      }
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

  auto max_length = this->traits.get_max_length();
  if (max_length > 0 && payload.size() > static_cast<size_t>(max_length)) {
    ESP_LOGW(TAG, "Dynamic lambda result truncated from %zu to %d characters", payload.size(), max_length);
    payload.resize(max_length);
  }

  this->lambda_result_->publish_state(payload);
}
#endif

}  // namespace template_
}  // namespace esphome

