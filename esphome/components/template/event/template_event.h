#pragma once

#include "esphome/core/component.h"
#include "esphome/components/event/event.h"

namespace esphome::template_ {

class TemplateEvent final : public Component, public event::Event {
 public:
  // User provided, not "= default": `new(p) TemplateEvent()` would zero-fill .bss that is already zero.
  TemplateEvent() {}
};

}  // namespace esphome::template_
