#pragma once

#include "esphome/core/component.h"
#include "esphome/components/event/event.h"

namespace esphome {
namespace template_ {

class TemplateEvent : public Component, public event::Event {};

}  // namespace template_
}  // namespace esphome
