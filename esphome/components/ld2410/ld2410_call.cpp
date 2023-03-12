#include "ld2410_call.h"
#include "ld2410.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ld2410 {

LD2410ComponentCall &LD2410ComponentCall::set_password(const std::string &password) {
  return with_operation_(LD2410_OP_SET_PASSWORD).with_password_(password);
}

LD2410ComponentCall &LD2410ComponentCall::with_operation_(LD2410ComponentOperation operation) {
  this->operation_ = operation;
  return *this;
}

LD2410ComponentCall &LD2410ComponentCall::with_password_(const std::string &password) {
  this->password_ = password;
  return *this;
}

void LD2410ComponentCall::perform() {
  auto *parent = this->parent_;

  if (this->operation_ == LD2410_OP_NONE) {
    ESP_LOGW(TAG, "LD2410ComponentCall performed without selecting an operation");
    return;
  }

  if (this->operation_ == LD2410_OP_SET_PASSWORD) {
    ESP_LOGD(TAG, "Setting password");
    if (!this->password_.has_value()) {
      ESP_LOGW(TAG, "No password value set for LD2410ComponentCall");
      return;
    }
    parent->set_bluetooth_password(this->password_.value());
  }
}

}  // namespace ld2410
}  // namespace esphome
