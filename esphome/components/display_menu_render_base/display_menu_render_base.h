#pragma once
#include "esphome/components/groups/groups.h"
#include "esphome/components/display_menu_base/menu_item.h"

namespace esphome {
namespace display_menu_render_base {

using namespace display_menu_base;

// Render interface for menu with groups or by entity type
class MenuRenderInterface : public groups::GroupsStorage {
 public:
  MenuRenderInterface(EntityType type = EntityType::NONE) : type_(type){};

  EntityType type() const { return type_; }
  void set_type(EntityType type) { this->type_ = type; }

  // Add needed items in menu with entity_info
  virtual size_t render_entity(MenuItemMenu *menu, EntityBase *entity) = 0;

  bool has_entity(EntityBase *entity) const {
    for (auto *group : this->groups_) {
      if (group->has_entity(entity))
        return true;
    }
    return false;
  }

 protected:
  EntityType type_;
};

}  // namespace display_menu_render_base
}  // namespace esphome
