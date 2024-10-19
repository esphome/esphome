#pragma once

namespace esphome {
namespace qn8027 {

#ifndef SUB_TEXT
#define SUB_TEXT(name) \
 protected: \
  text::Text *name##_text_{nullptr}; \
\
 public: \
  void set_##name##_text(text::Text *text) { this->name##_text_ = text; }
#endif

#define QN8027_SUB_NUMBER(name, type) \
  SUB_NUMBER(name) \
  void publish_##name() { this->publish(this->name##_number_, (float) this->get_##name()); } \
  void set_##name(type value); \
  type get_##name();

#define QN8027_SUB_SWITCH(name) \
  SUB_SWITCH(name) \
  void publish_##name() { this->publish_switch(this->name##_switch_, this->get_##name()); } \
  void set_##name(bool value); \
  bool get_##name();

#define QN8027_SUB_SELECT(name, type) \
  SUB_SELECT(name) \
  void publish_##name() { this->publish_select(this->name##_select_, (size_t) this->get_##name()); } \
  void set_##name(type value); \
  type get_##name();

#define QN8027_SUB_TEXT(name) \
  SUB_TEXT(name) \
  void publish_##name() { this->publish(this->name##_text_, this->get_##name()); } \
  void set_##name(const std::string &value); \
  std::string get_##name();

#define QN8027_SUB_SENSOR(name) \
  SUB_SENSOR(name) \
  void publish_##name##_sensor() { this->publish(this->name##_sensor_, (float) this->get_##name##_sensor()); } \
  float get_##name##_sensor();

#define QN8027_SUB_BINARY_SENSOR(name) \
  SUB_BINARY_SENSOR(name) \
  void publish_##name##_binary_sensor() { \
    this->publish(this->name##_binary_sensor_, this->get_##name##_binary_sensor()); \
  } \
  bool get_##name##_binary_sensor();

#define QN8027_SUB_TEXT_SENSOR(name) \
  SUB_TEXT_SENSOR(name) \
  void publish_##name##_text_sensor() { this->publish(this->name##_text_sensor_, this->get_##name##_text_sensor()); } \
  std::string get_##name##_text_sensor();

}  // namespace qn8027
}  // namespace esphome
