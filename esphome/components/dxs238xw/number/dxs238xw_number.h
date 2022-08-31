#pragma once

#include "../dxs238xw.h"
#include "esphome/components/number/number.h"

namespace esphome {
namespace dxs238xw {

class Dxs238xwNumber : public number::Number {
 public:
  void set_entity_id(SmIdEntity entity_id) { this->entity_id_ = entity_id; }
  void set_dxs238xw_parent(Dxs238xwComponent *parent) { this->parent_ = parent; }

 protected:
  void control(float value) override;

  Dxs238xwComponent *parent_;
  SmIdEntity entity_id_ = SmIdEntity::ID_NULL;
};

}  // namespace dxs238xw
}  // namespace esphome
