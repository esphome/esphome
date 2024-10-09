#pragma once

#include "esphome/components/text/text.h"
#include "../qn8027.h"

namespace esphome {
namespace qn8027 {

class RDSTextText : public text::Text, public Parented<QN8027Component> {
 public:
  RDSTextText() = default;

 protected:
  void control(const std::string &value) override;
};

}  // namespace qn8027
}  // namespace esphome
