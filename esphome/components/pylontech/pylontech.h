#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/line_reader.h"
#include "esphome/components/uart/uart.h"

namespace esphome::pylontech {

static const uint8_t TEXT_SENSOR_MAX_LEN = 14;

class PylontechListener {
 public:
  struct LineContents {
    int bat_num = 0, volt, curr, tempr, tlow, thigh, vlow, vhigh, coulomb, mostempr;
    char base_st[TEXT_SENSOR_MAX_LEN] = {0}, volt_st[TEXT_SENSOR_MAX_LEN] = {0}, curr_st[TEXT_SENSOR_MAX_LEN] = {0},
         temp_st[TEXT_SENSOR_MAX_LEN] = {0};
  };

  virtual void on_line_read(LineContents *line) = 0;
  virtual void dump_config() = 0;
};

class PylontechComponent final : public PollingComponent, public uart::UARTDevice {
 public:
  PylontechComponent();

  /// Schedule data readings.
  void update() override;
  /// Read data once available
  void loop() override;
  /// Setup the sensor and test for a connection.
  void setup() override;
  void dump_config() override;

#ifdef PYLONTECH_LISTENER_COUNT
  void register_listener(PylontechListener *listener, uint8_t battery) {
    this->listeners_.push_back(listener);
    this->batteries_ |= 1u << (battery - 1);
  }

 protected:
  void process_line_(const char *buffer);
  void dispatch_one_();

  // The longest line, the newer firmware header, is 182 characters
  uart::LineReader<200> line_reader_;
  StaticVector<PylontechListener *, PYLONTECH_LISTENER_COUNT> listeners_;
  // Parsed while the UART drains, published one per loop once it is idle
  StaticVector<PylontechListener::LineContents, PYLONTECH_BATTERY_COUNT> queue_;
  uint16_t batteries_{0};  // bit n set: battery n + 1 has a listener
  uint8_t dispatched_{0};
  uint8_t dropped_{0};
  bool has_tlow_id_{false};
#endif
};

}  // namespace esphome::pylontech
