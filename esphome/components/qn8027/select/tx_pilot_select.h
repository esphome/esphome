#pragma once

#include "esphome/components/select/select.h"
#include "../qn8027.h"

namespace esphome {
namespace qn8027 {

class TxPilotSelect : public select::Select, public Parented<QN8027Component> {
 public:
  TxPilotSelect() = default;

 protected:
  void control(const std::string &value) override;
};

}  // namespace qn8027
}  // namespace esphome
