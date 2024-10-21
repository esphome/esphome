#pragma once

#include "esphome/components/text/text.h"
#include "../qn8027.h"

namespace esphome {
namespace qn8027 {

class RDSStationText : public text::Text, public Parented<QN8027Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    this->parent_->set_rds_station(value);
  }
};

class RDSTextText : public text::Text, public Parented<QN8027Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    this->parent_->set_rds_text(value);
  }
};
}  // namespace qn8027
}  // namespace esphome
