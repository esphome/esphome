#pragma once

#include "esphome/core/component.h"
#include "esphome/components/event/event.h"

namespace esphome {
namespace template_ {

class TemplateEvent : public Component, public event::Event {
 public:
  void event_fired_action(const std::string &event_type) override{};
};

}  // namespace template_
}  // namespace esphome
