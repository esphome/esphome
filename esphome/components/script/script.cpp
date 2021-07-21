#include "script.h"
#include "esphome/core/log.h"

namespace esphome {
namespace script {

static const char *const TAG = "script";

void SingleScript::execute() {
  if (this->is_action_running()) {
    ESP_LOGW(TAG, "Script '%s' is already running! (mode: single)", this->name_.c_str());
    return;
  }

  this->trigger();
}

void RestartScript::execute() {
  if (this->is_action_running()) {
    ESP_LOGD(TAG, "Script '%s' restarting (mode: restart)", this->name_.c_str());
    this->stop_action();
  }

  this->trigger();
}

void QueueingScript::execute() {
  if (this->is_action_running()) {
    // num_runs_ is the number of *queued* instances, so total number of instances is
    // num_runs_ + 1
    if (this->max_runs_ != 0 && this->num_runs_ + 1 >= this->max_runs_) {
      ESP_LOGW(TAG, "Script '%s' maximum number of queued runs exceeded!", this->name_.c_str());
      return;
    }

    ESP_LOGD(TAG, "Script '%s' queueing new instance (mode: queued)", this->name_.c_str());
    this->num_runs_++;
    return;
  }

  this->trigger();
  // Check if the trigger was immediate and we can continue right away.
  this->loop();
}

void QueueingScript::stop() {
  this->num_runs_ = 0;
  Script::stop();
}

void QueueingScript::loop() {
  if (this->num_runs_ != 0 && !this->is_action_running()) {
    this->num_runs_--;
    this->trigger();
  }
}

void ParallelScript::execute() {
  if (this->max_runs_ != 0 && this->automation_parent_->num_running() >= this->max_runs_) {
    ESP_LOGW(TAG, "Script '%s' maximum number of parallel runs exceeded!", this->name_.c_str());
    return;
  }
  this->trigger();
}

}  // namespace script
}  // namespace esphome
