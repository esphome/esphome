#pragma once

#include "esphome/components/select/select.h"

#include "../duco.h"

namespace esphome::duco {

class DucoNightboost : public DucoDevice, public select::Select {
 public:
  void update() override;
  void receive_response(const DucoMessage &message) override;

 protected:
  void control(size_t index) override;
};

class DucoNightboostMonthStart : public DucoDevice, public select::Select {
 public:
  void update() override;
  void receive_response(const DucoMessage &message) override;

 protected:
  void control(size_t index) override;
};

class DucoNightboostMonthStop : public DucoDevice, public select::Select {
 public:
  void update() override;
  void receive_response(const DucoMessage &message) override;

 protected:
  void control(size_t index) override;
};

}  // namespace esphome::duco
