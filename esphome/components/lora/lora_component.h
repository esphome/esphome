#pragma once
#include "esphome/core/component.h"

namespace esphome {
namespace lora {
class LoraComponent : public Component {
  virtual void send_printf(const char *format, ...) __attribute__((format(printf, 2, 3)));
};
}  // namespace lora
}  // namespace esphome
