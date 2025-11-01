#pragma once

#include "esphome/core/component.h"
#include "esphome/components/miio/miio.h"
#include "esphome/components/select/select.h"

#include <vector>

namespace esphome {
namespace miio {

class MiioSelect : public select::Select, public Component {
 public:
  void setup() override;
  void dump_config() override;

  void set_miio_parent(Miio *parent) { this->parent_ = parent; }
  void set_optimistic(bool optimistic) { this->optimistic_ = optimistic; }
  void set_select_id(uint8_t cluster, uint8_t key) { this->select_id_ = std::make_tuple(cluster, key); }
  void set_select_mappings(std::vector<uint8_t> mappings) { this->mappings_ = std::move(mappings); }

 protected:
  void control(const std::string &value) override;

  Miio *parent_;
  bool optimistic_ = false;
  prop_t select_id_;
  std::vector<uint8_t> mappings_;
};

}  // namespace miio
}  // namespace esphome
