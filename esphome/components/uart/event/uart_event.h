#pragma once

#include "esphome/core/component.h"
#include "esphome/components/event/event.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/log.h"
#include <vector>
#include <string>
#include <map>

namespace esphome {
namespace uart {

class UARTEvent : public event::Event, public UARTDevice, public Component {
 protected:
  struct EventMatcher {
    std::string event_name;
    std::vector<uint8_t> binary_data;
  };

  void read_data();
  std::vector<EventMatcher> matchers_;
  std::vector<uint8_t> buffer_;
  size_t max_matcher_len_ = 0;

 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void add_event_matcher(const std::string &event_name, const std::string &match_string);
  void add_event_matcher(const std::string &event_name, const std::vector<uint8_t> &match_binary);
};

}  // namespace uart
}  // namespace esphome
