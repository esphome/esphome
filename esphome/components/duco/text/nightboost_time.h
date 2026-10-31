#pragma once

#include "esphome/components/text/text.h"

#include "../duco.h"

namespace esphome::duco {

class DucoNightboostTimeStart : public DucoDevice, public text::Text {
 public:
  void update() override;
  void receive_response(const DucoMessage &message) override;

 protected:
  void control(const std::string &value) override;
};

class DucoNightboostTimeStop : public DucoDevice, public text::Text {
 public:
  void update() override;
  void receive_response(const DucoMessage &message) override;

 protected:
  void control(const std::string &value) override;
};

}  // namespace esphome::duco
