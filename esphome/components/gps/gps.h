#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include <TinyGPS++.h>

namespace esphome {
namespace gps {

class GPS;

class GPSListener {
 public:
  virtual void on_update(TinyGPSPlus &tiny_gps) = 0;
  TinyGPSPlus &get_tiny_gps();

 protected:
  friend GPS;

  GPS *parent_;
};

class GPS : public Component, public uart::UARTDevice {
 public:
  void register_listener(GPSListener *listener) {
    listener->parent_ = this;
    this->listeners_.push_back(listener);
  }
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void loop() override {
    while (this->available() && !this->has_time_) {
      if (this->tiny_gps_.encode(this->read())) {
        for (auto *listener : this->listeners_)
          listener->on_update(this->tiny_gps_);
      }
    }
  }
  TinyGPSPlus &get_tiny_gps() { return this->tiny_gps_; }

 protected:
  bool has_time_{false};
  TinyGPSPlus tiny_gps_;
  std::vector<GPSListener *> listeners_{};
};

}  // namespace gps
}  // namespace esphome
