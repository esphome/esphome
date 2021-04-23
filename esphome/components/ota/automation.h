#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "ota_component.h"

namespace esphome {
namespace ota {

class OTAStartTrigger : public Trigger<> {
 public:
  explicit OTAStartTrigger(OTAComponent *parent) {
    parent->add_on_begin_callback([this]() { trigger(); });
  }
};

class OTAEndTrigger : public Trigger<> {
 public:
  explicit OTAEndTrigger(OTAComponent *parent) {
    parent->add_on_end_callback([this]() { trigger(); });
  }
};

class OTAProgressTrigger : public Trigger<float> {
 public:
  explicit OTAProgressTrigger(OTAComponent *parent) {
    parent->add_on_progress_callback([this](float progress) { trigger(progress); });
  }
};

class OTAErrorTrigger : public Trigger<int> {
 public:
  explicit OTAErrorTrigger(OTAComponent *parent) {
    parent->add_on_error_callback([this](int error_code) { trigger(error_code); });
  }
};

}  // namespace ota
}  // namespace esphome
