#include "display_menu_render.h"

namespace esphome {
namespace display_menu_render_base {

#ifdef USE_SENSOR
size_t SensorMenuRender::render_entity(MenuItemMenu *menu, EntityBase *entity) {
  sensor::Sensor *sensor_obj = static_cast<sensor::Sensor *>(entity);
  MenuItemValue *item = new MenuItemValue();
  item->set_text(sensor_obj->get_name());
  item->set_value_lambda([sensor_obj, this](const display_menu_base::MenuItem *it) -> std::string {
    return sensor_obj->state_to_string(this->no_data_text_.c_str());
  });
  menu->add_generated_items(item);
  return 1;
}
#endif

#ifdef USE_SWITCH
size_t SwitchMenuRender::render_entity(MenuItemMenu *menu, EntityBase *entity) {
  switch_::Switch *switch_obj = static_cast<switch_::Switch *>(entity);
  MenuItemSwitch *menu_switch = new MenuItemSwitch();
  menu_switch->set_text(switch_obj->get_name());
  menu_switch->set_immediate_edit(true);
  menu_switch->set_switch_variable(switch_obj);
  menu_switch->set_on_text(this->on_text_.c_str());
  menu_switch->set_off_text(this->off_text_.c_str());

  menu->add_generated_items(menu_switch);
  return 1;
}
#endif

#ifdef USE_BINARY_SENSOR
size_t BinarySensorMenuRender::render_entity(MenuItemMenu *menu, EntityBase *entity) {
  auto *sensor_obj = static_cast<binary_sensor::BinarySensor *>(entity);
  MenuItemBinarySensor *item = new MenuItemBinarySensor();

  item->set_text(sensor_obj->get_name());
  item->set_binary_sensor_variable(sensor_obj);
  item->set_on_text(this->on_text_.c_str());
  item->set_off_text(this->off_text_.c_str());
  item->set_no_data_text(this->no_data_text_.c_str());

  menu->add_generated_items(item);
  return 1;
}
#endif

}  // namespace display_menu_render_base
}  // namespace esphome
