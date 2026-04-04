#pragma once

#include "esphome/core/component.h"
#include "esphome/components/event/event.h"

namespace esphome::template_ {

class TemplateEvent final : public Component, public event::Event {};

}  // namespace esphome::template_
