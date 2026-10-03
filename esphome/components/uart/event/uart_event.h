#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/components/event/event.h"
#include "esphome/components/uart/uart.h"
#include <type_traits>
#include <vector>

namespace esphome::uart {

/// An event type and the byte pattern that fires it; `data` may point to PROGMEM.
struct UARTEventMatcher {
  const char *event_name;
  const uint8_t *data;
  size_t data_len;
};
// Codegen keeps the matchers in a flash table read with plain loads, which ESP8266 only allows for whole words.
static_assert(std::is_same_v<decltype(UARTEventMatcher::event_name), const char *>,
              "UARTEventMatcher fields must stay word sized");
static_assert(std::is_same_v<decltype(UARTEventMatcher::data), const uint8_t *>,
              "UARTEventMatcher fields must stay word sized");
static_assert(std::is_same_v<decltype(UARTEventMatcher::data_len), size_t>,
              "UARTEventMatcher fields must stay word sized");
#ifdef USE_ESP8266
static_assert(sizeof(UARTEventMatcher) == 3 * sizeof(uint32_t), "UARTEventMatcher is read from flash with word loads");
#endif

class UARTEvent final : public event::Event, public UARTDevice, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  /// `matchers` is a codegen PROGMEM table that must outlive the component.
  void set_matchers(const UARTEventMatcher *matchers, uint16_t count, uint16_t max_len) {
    this->matchers_ = matchers;
    this->matcher_count_ = count;
    this->max_matcher_len_ = max_len;
  }

 protected:
  void read_data_();
  const UARTEventMatcher *matchers_{nullptr};
  std::vector<uint8_t> buffer_;
  uint16_t matcher_count_{0};
  uint16_t max_matcher_len_{0};
};

}  // namespace esphome::uart
