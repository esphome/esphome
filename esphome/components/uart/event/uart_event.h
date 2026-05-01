#pragma once

#include "esphome/core/component.h"
#include "esphome/components/event/event.h"
#include "esphome/components/uart/uart.h"
#include <vector>

namespace esphome::uart {

class UARTEvent : public event::Event, public UARTDevice, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void add_event_matcher(const char *event_name, const uint8_t *match_data, size_t match_data_len);

 protected:
  struct EventMatcher {
    const char *event_name;
    const uint8_t *data;
    size_t data_len;
  };

  void read_data_();
  std::vector<EventMatcher> matchers_;
  std::vector<uint8_t> buffer_;
  size_t max_matcher_len_ = 0;
};

}  // namespace esphome::uart
