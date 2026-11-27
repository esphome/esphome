#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include <TinyGPSPlus.h>

namespace esphome::gps {

class GPS;

class GPSListener {
 public:
  virtual void on_update(TinyGPSPlus &tiny_gps) = 0;
  TinyGPSPlus &get_tiny_gps();

 protected:
  friend GPS;

  GPS *parent_;
};

class GPS final : public PollingComponent, public uart::UARTDevice {
  SUB_SENSOR(latitude)
  SUB_SENSOR(longitude)
  SUB_SENSOR(speed)
  SUB_SENSOR(course)
  SUB_SENSOR(altitude)
  SUB_SENSOR(satellites)
  SUB_SENSOR(hdop)

 public:
  SLOT_LIST_REQUIRES(GPS_LISTENER_COUNT)
  void register_listener(GPSListener *listener) {
    listener->parent_ = this;
    this->listeners_.push_back(listener);
  }
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void dump_config() override;
  void loop() override;
  void update() override;

  TinyGPSPlus &get_tiny_gps() { return this->tiny_gps_; }

 protected:
  float latitude_{NAN};
  float longitude_{NAN};
  float speed_{NAN};
  float course_{NAN};
  float altitude_{NAN};
  float hdop_{NAN};
  uint16_t satellites_{0};
  bool has_time_{false};

  TinyGPSPlus tiny_gps_;
  [[no_unique_address]] SlotList<GPSListener *, GPS_LISTENER_COUNT> listeners_;
};

}  // namespace esphome::gps
