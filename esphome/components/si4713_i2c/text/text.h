#pragma once

#include "esphome/components/text/text.h"
#include "../si4713.h"

namespace esphome {
namespace si4713_i2c {

class RDSStationText : public text::Text, public Parented<Si4713Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    this->parent_->set_rds_station(value);
  }
};

class RDSTextText : public text::Text, public Parented<Si4713Component> {
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    this->parent_->set_rds_text(value);
  }
};

}  // namespace si4713_i2c
}  // namespace esphome
