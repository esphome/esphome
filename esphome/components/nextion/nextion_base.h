#pragma once
#include "esphome/core/defines.h"
#include "esphome/core/color.h"
#include "nextion_component_base.h"
namespace esphome {
namespace nextion {

#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
#define NEXTION_PROTOCOL_LOG
#endif

#ifdef NEXTION_PROTOCOL_LOG
#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
#define ESP_LOGN(tag, ...) ESP_LOGVV(tag, __VA_ARGS__)
#else
#define ESP_LOGN(tag, ...) ESP_LOGD(tag, __VA_ARGS__)
#endif
#else
#define ESP_LOGN(tag, ...) \
  {}
#endif

class NextionBase;

class NextionBase {
 public:
  virtual void add_no_result_to_queue_with_set(NextionComponentBase *component, int state_value) = 0;
  virtual void add_no_result_to_queue_with_set(const std::string &variable_name,
                                               const std::string &variable_name_to_send, int state_value) = 0;

  virtual void add_no_result_to_queue_with_set(NextionComponentBase *component, const std::string &state_value) = 0;
  virtual void add_no_result_to_queue_with_set(const std::string &variable_name,
                                               const std::string &variable_name_to_send,
                                               const std::string &state_value) = 0;

  virtual void add_addt_command_to_queue(NextionComponentBase *component) = 0;

  virtual void add_to_get_queue(NextionComponentBase *component) = 0;

  virtual void set_component_background_color(const char *component, Color color) = 0;
  virtual void set_component_pressed_background_color(const char *component, Color color) = 0;
  virtual void set_component_font_color(const char *component, Color color) = 0;
  virtual void set_component_pressed_font_color(const char *component, Color color) = 0;
  virtual void set_component_font(const char *component, uint8_t font_id) = 0;

  virtual void show_component(const char *component) = 0;
  virtual void hide_component(const char *component) = 0;

  bool is_sleeping() { return this->is_sleeping_; }
  bool is_setup() { return this->is_setup_; }

 protected:
  bool is_setup_ = false;
  bool is_sleeping_ = false;
};

}  // namespace nextion
}  // namespace esphome
