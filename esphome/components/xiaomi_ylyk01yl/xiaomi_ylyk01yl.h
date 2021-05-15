#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/xiaomi_ble/xiaomi_ble.h"
#include "esphome/core/automation.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace xiaomi_ylyk01yl {

static const uint8_t BUTTON_ON = 0;
static const uint8_t BUTTON_OFF = 1;
static const uint8_t BUTTON_SUN = 2;
static const uint8_t BUTTON_M = 4;
static const uint8_t BUTTON_PLUS = 3;
static const uint8_t BUTTON_MINUS = 5;

class XiaomiYLYK01YL : public Component, public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  void set_address(uint64_t address) { address_ = address; }

  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;

  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void set_keycode(sensor::Sensor *keycode) { keycode_ = keycode; }
  void add_on_receive_callback(std::function<void(int)> &&callback);

 protected:
  uint64_t address_;
  sensor::Sensor *keycode_{nullptr};
  CallbackManager<void(int)> receive_callback_{};
};

class OnButtonOnTrigger : public Trigger<> {
 public:
  OnButtonOnTrigger(XiaomiYLYK01YL *a_remote) {
    a_remote->add_on_receive_callback([this](int keycode) {
      if (keycode == BUTTON_ON) {
        this->trigger();
      }
    });
  }
};

class OnButtonOffTrigger : public Trigger<> {
 public:
  OnButtonOffTrigger(XiaomiYLYK01YL *a_remote) {
    a_remote->add_on_receive_callback([this](int keycode) {
      if (keycode == BUTTON_OFF) {
        this->trigger();
      }
    });
  }
};

class OnButtonSunTrigger : public Trigger<> {
 public:
  OnButtonSunTrigger(XiaomiYLYK01YL *a_remote) {
    a_remote->add_on_receive_callback([this](int keycode) {
      if (keycode == BUTTON_SUN) {
        this->trigger();
      }
    });
  }
};

class OnButtonMTrigger : public Trigger<> {
 public:
  OnButtonMTrigger(XiaomiYLYK01YL *a_remote) {
    a_remote->add_on_receive_callback([this](int keycode) {
      if (keycode == BUTTON_M) {
        this->trigger();
      }
    });
  }
};

class OnButtonPlusTrigger : public Trigger<> {
 public:
  OnButtonPlusTrigger(XiaomiYLYK01YL *a_remote) {
    a_remote->add_on_receive_callback([this](int keycode) {
      if (keycode == BUTTON_PLUS) {
        this->trigger();
      }
    });
  }
};

class OnButtonMinusTrigger : public Trigger<> {
 public:
  OnButtonMinusTrigger(XiaomiYLYK01YL *a_remote) {
    a_remote->add_on_receive_callback([this](int keycode) {
      if (keycode == BUTTON_MINUS) {
        this->trigger();
      }
    });
  }
};

}  // namespace xiaomi_ylyk01yl
}  // namespace esphome

#endif
