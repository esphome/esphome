#include "template_text.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <string>

namespace esphome {
namespace template_ {

static const char *const TAG = "template.text";

void print(WRContext *c, const WRValue *argv, const int argn, WRValue &retVal, void *usr) {
  char buf[255];
  for (int i = 0; i < argn; ++i) {
    printf("%s", argv[i].asString(buf, 255));
  }
}

void switches(WRContext *c, const WRValue *argv, const int argn, WRValue &retVal, void *usr) {
  std::vector<switch_::Switch *> sw = App.get_switches();
  char buf[255];

  for (int i = 0; i < argn; ++i) {
    printf("%s", argv[i].asString(buf, 255));
  }

  for (unsigned int i = 0; i < sw.size(); i++) {
    ESP_LOGD(TAG, "switch: %s", sw[i]->get_name().c_str());
    ESP_LOGD(TAG, "switch: %i", sw[i]->state);
    if (strcmp(sw[i]->get_name().c_str(), buf) == 0) {
      wr_makeInt(&retVal, sw[i]->state);
    }
  };
}

void TemplateText::setup() {
  if (!(this->f_ == nullptr)) {
    if (this->f_.has_value())
      return;
  }

  std::string value;
  ESP_LOGD(TAG, "Setting up Template Text Input");
  value = this->initial_value_;
  if (!this->pref_) {
    ESP_LOGD(TAG, "State from initial: %s", value.c_str());
  } else {
    uint32_t key = this->get_object_id_hash();
    key += this->traits.get_min_length() << 2;
    key += this->traits.get_max_length() << 4;
    key += fnv1_hash(this->traits.get_pattern()) << 6;
    this->pref_->setup(key, value);
  }
  if (!value.empty())
    this->publish_state(value);

  if (this->dynamic_) {
    ESP_LOGD(TAG, "Initiate wrench");
    this->w = wr_newState();
    wr_registerFunction(this->w, "print", print);
    wr_registerFunction(this->w, "switches", switches);
  }
}

void TemplateText::update() {
  ESP_LOGD(TAG, "update");

  if (this->dynamic_) {
    this->execute_wrench(this->state);
  }
  if (this->f_ == nullptr)
    return;

  if (!this->f_.has_value())
    return;

  auto val = (*this->f_)();
  if (!val.has_value())
    return;

  this->publish_state(*val);
}

void TemplateText::execute_wrench(const std::string &value) {
  int err = wr_compile(value.c_str(), strlen(value.c_str()), &this->outBytes, &this->outLen);  // compile it

  ESP_LOGD(TAG, "wrench err: %i", err);
  if (err == 0) {
    WRContext *gc = wr_run(this->w, this->outBytes, this->outLen);  // load and run the code!

    WRValue *g = wr_getGlobalRef(gc, "result");

    if (g) {
      char buf[1024];
      this->lambda_result_->publish_state(g->asString(buf, 1024));
    }

    delete[] outBytes;  // clean up
  }
}

void TemplateText::control(const std::string &value) {
  this->set_trigger_->trigger(value);

  if (this->dynamic_) {
    this->execute_wrench(value);
  }

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
  ESP_LOGCONFIG(TAG, "  Dynamic: %s", YESNO(this->dynamic_));
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace template_
}  // namespace esphome
