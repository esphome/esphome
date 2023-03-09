#pragma once

#include "esphome/core/helpers.h"

namespace esphome {
namespace ld2410 {

class LD2410Component;

enum LD2410ComponentOperation { LD2410_OP_NONE, LD2410_OP_SET_PASSWORD };

class LD2410ComponentCall {
 public:
  explicit LD2410ComponentCall(LD2410Component *parent) : parent_(parent) {}
  void perform();

  LD2410ComponentCall &set_password(const std::string &password);

 protected:
  LD2410Component *const parent_;
  optional<std::string> password_;
  LD2410ComponentOperation operation_{LD2410_OP_NONE};

  LD2410ComponentCall &with_operation_(LD2410ComponentOperation operation);
  LD2410ComponentCall &with_password_(const std::string &password);
};

}  // namespace ld2410
}  // namespace esphome
