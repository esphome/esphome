#pragma once
#include "esphome/core/defines.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "keyboard_call.h"
#include <vector>

namespace esphome {
namespace keyboard {

enum KeyboardType : uint8_t {
  KEYBOARD,
  MEDIA_KEYS,
};

class KeyboardControl {
 protected:
  friend class Keyboard;
  friend class KeyboardCall;
  virtual void control(std::vector<uint16_t> &&keys) = 0;
  std::vector<uint16_t> *keys_{nullptr};
};

class LEDControl {
 public:
#ifdef USE_BINARY_SENSOR
  virtual void publish_capslock(bool state) = 0;
  virtual void publish_numlock(bool state) = 0;
  virtual void publish_scrollock(bool state) = 0;
#endif
};

class Keyboard : public EntityBase, public Component, public LEDControl {
 public:
  Keyboard(KeyboardControl *keyboard_control, KeyboardControl *media_keys_control);
  std::vector<uint16_t> keyboard_keys;
  std::vector<uint16_t> media_keys;
#ifdef USE_BINARY_SENSOR
  void set_capslock_binary_sensor(binary_sensor::BinarySensor *capslock);
  void set_numlock_binary_sensor(binary_sensor::BinarySensor *numlock);
  void set_scrollock_binary_sensor(binary_sensor::BinarySensor *scrollock);
  void publish_capslock(bool state) override;
  void publish_numlock(bool state) override;
  void publish_scrollock(bool state) override;
#endif
  void add_on_state_callback(std::function<void()> &&callback);

  KeyboardCall make_call(KeyboardType type);

 protected:
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *capslock_{nullptr};
  binary_sensor::BinarySensor *numlock_{nullptr};
  binary_sensor::BinarySensor *scrollock_{nullptr};
#endif
  CallbackManager<void()> state_callback_{};

  KeyboardControl *keyboard_control_;
  KeyboardControl *media_keys_control_;
};

class Keys {
 public:
  void set_key(std::vector<uint16_t> &&keys) { this->keys_ = std::move(keys); }
  void set_type(KeyboardType type) { type_ = type; }

 protected:
  std::vector<uint16_t> keys_;
  KeyboardType type_{KEYBOARD};
};

template<typename... Ts> class SetAction : public Action<Ts...>, public Parented<Keyboard>, public Keys {
 public:
  void play(Ts... x) override { this->parent_->make_call(type_).set_key(keys_).perform(); }
};

template<typename... Ts> class DownAction : public Action<Ts...>, public Parented<Keyboard>, public Keys {
 public:
  void play(Ts... x) override { this->parent_->make_call(type_).key_down(keys_).perform(); }
};

template<typename... Ts> class UpAction : public Action<Ts...>, public Parented<Keyboard>, public Keys {
 public:
  void play(Ts... x) override { this->parent_->make_call(type_).key_up(keys_).perform(); }
};

}  // namespace keyboard
}  // namespace esphome
