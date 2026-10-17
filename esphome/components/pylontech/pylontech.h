#pragma once

#include <array>
#include <cstdint>

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/line_reader.h"
#include "esphome/components/uart/uart.h"

namespace esphome::pylontech {

static constexpr uint8_t TEXT_SENSOR_MAX_LEN = 14;
// Battery numbers run 1 to MAX_BATTERIES; bit n - 1 of a battery_mask_t stands for battery n
static constexpr uint8_t MAX_BATTERIES = 16;
using battery_mask_t = uint16_t;
static_assert(MAX_BATTERIES <= sizeof(battery_mask_t) * 8, "battery mask too narrow");

class PylontechListener {
 public:
  // No default initializers: the hub's slots stay plain zeroed storage; a parse writes every field
  struct LineContents {
    int32_t curr, tempr, tlow, thigh, mostempr;
    uint16_t volt, vlow, vhigh;  // mV
    uint8_t bat_num, coulomb;
    char base_st[TEXT_SENSOR_MAX_LEN], volt_st[TEXT_SENSOR_MAX_LEN], curr_st[TEXT_SENSOR_MAX_LEN],
        temp_st[TEXT_SENSOR_MAX_LEN];
  };

  explicit PylontechListener(uint8_t bat_num) : bat_num_(bat_num) {}
  uint8_t get_battery() const { return this->bat_num_; }

  /// Called only with records for this listener's battery.
  virtual void on_line_read(LineContents *line) = 0;
  virtual void dump_config() = 0;

 protected:
  uint8_t bat_num_;
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
  void register_listener(PylontechListener *listener) {
    this->listeners_.push_back(listener);
    this->batteries_ |= 1u << (listener->get_battery() - 1);
  }

 protected:
  void process_line_(const char *buffer);
  void parse_header_(const char *buffer);
  void dispatch_one_();

  // The longest line, the newer firmware header, is 182 characters
  uart::LineReader<200> line_reader_;
  StaticVector<PylontechListener *, PYLONTECH_LISTENER_COUNT> listeners_;
  // One slot per listened battery, in battery order; parsed while the UART drains, published one per
  // loop once it is idle. A newer line for a battery replaces the unpublished one.
  static_assert(PYLONTECH_BATTERY_COUNT <= MAX_BATTERIES, "more slots than batteries");
  std::array<PylontechListener::LineContents, PYLONTECH_BATTERY_COUNT> slots_;
  battery_mask_t batteries_{0};  // batteries with a listener
  battery_mask_t pending_{0};    // bit i set: slots_[i] is waiting to be published
  bool has_tlow_id_{false};
#endif
};

}  // namespace esphome::pylontech
