#pragma once

#include "display_menu_render_base.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif

namespace esphome {
namespace display_menu_render_base {

using namespace display_menu_base;

#ifdef USE_SENSOR

// Create lambda to generate text for menu_item
template<bool with_name>
auto make_sensor_lambda(sensor::Sensor *sensor_obj, const char *no_data_text, int8_t accuracy = -1) {
  auto lambda = [=](const display_menu_base::MenuItem *it) -> std::string {
    char buf[50];
    char format_buf[10];
    int symb_nums = 0;
    if (with_name)
      symb_nums += sprintf(buf, "%s: ", sensor_obj->get_name().c_str());

    int8_t int_accuracy = (accuracy >= 0) ? accuracy : sensor_obj->get_accuracy_decimals();
    sprintf(format_buf, "%%0.%df", int_accuracy);

    if (sensor_obj->has_state()) {
      sprintf(buf + symb_nums, format_buf, sensor_obj->state);
    } else {
      sprintf(buf + symb_nums, no_data_text);
    }
    return buf;
  };
  return lambda;
};

// Default render for sensor objects
class SensorMenuRender : public MenuRenderInterface {
 public:
  SensorMenuRender() : MenuRenderInterface(EntityType::SENSOR) {}
  size_t render_entity(MenuItemMenu *menu, EntityBase *entity) override;

  void set_accuracy(int accuracy) { this->accuracy_ = accuracy; }
  void set_no_data_text(const char *text) { this->no_data_text_ = StringRef(text); }

 protected:
  int accuracy_;
  StringRef no_data_text_;
};
#endif

#ifdef USE_SWITCH

// Default render for switch objects
class SwitchMenuRender : public MenuRenderInterface {
 public:
  SwitchMenuRender() : MenuRenderInterface(EntityType::SWITCH) {}
  size_t render_entity(MenuItemMenu *menu, EntityBase *entity) override;

  void set_on_text(const char *text) { this->on_text_ = StringRef(text); }
  void set_off_text(const char *text) { this->off_text_ = StringRef(text); }

 protected:
  StringRef on_text_;
  StringRef off_text_;
};
#endif

// Render with lambda
class LambdaMenuRender : public MenuRenderInterface {
 public:
  // Render lambda should return num of added elements in menu args
  using render_lambda_t = std::function<size_t(MenuItemMenu *menu, EntityBase *entity)>;

  LambdaMenuRender() : MenuRenderInterface(EntityType::NONE) {}
  size_t render_entity(MenuItemMenu *menu, EntityBase *entity) override {
    if (lambda_)
      return lambda_(menu, entity);
    return 0;
  }
  void set_lambda(render_lambda_t &&lambda) { this->lambda_ = lambda; }
  render_lambda_t lambda_;
};

}  // namespace display_menu_render_base
}  // namespace esphome
