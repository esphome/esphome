#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

#include "esphome/components/zigbee/zigbee_endpoint_callbacks.h"
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
// The component is final; expose its table only to verify retained handlers.
#define protected public
#include "esphome/components/zigbee/zigbee_zephyr.h"
#undef protected
#include "esphome/core/log.h"

static std::vector<std::string> errors;

namespace esphome {
void esp_log_printf_(int level, const char *tag, int line, const char *format, ...) {
  assert(level == ESPHOME_LOG_LEVEL_ERROR);
  assert(std::string(tag) == "zigbee");
  (void) line;
  char message[256];
  va_list args;
  va_start(args, format);
  vsnprintf(message, sizeof(message), format, args);
  va_end(args);
  errors.emplace_back(message);
}
}  // namespace esphome

// SDK lifecycle is not exercised by this host registration test.
namespace esphome::zigbee {
void ZigbeeComponent::setup() {}
void ZigbeeComponent::dump_config() {}
void ZigbeeComponent::loop() {}
}  // namespace esphome::zigbee

int main() {
  esphome::zigbee::ZigbeeComponent component;
  int first = 0;
  int second = 0;
  auto rejected = [&](uint8_t endpoint) {
    const auto before = errors.size();
    component.add_callback(endpoint, [&](uint8_t) { first = -100; });
    assert(errors.size() == before + 1);
    assert(errors.back().find("callback") != std::string::npos);
    char endpoint_text[16];
    snprintf(endpoint_text, sizeof(endpoint_text), "endpoint %u:", endpoint);
    assert(errors.back().find(endpoint_text) != std::string::npos);
  };
  rejected(0);
  rejected(240);
  rejected(255);
  errors.clear();
  component.add_callback(17, [&](uint8_t value) { first += value; });
  assert(errors.empty());
  rejected(17);  // Do not replace the first handler.
  component.add_callback(239, [&](uint8_t value) { second += value; });
  assert(errors.size() == 1);
  rejected(1);  // Fixed capacity is two, regardless of the largest ID.
  assert(component.callbacks_.dispatch(17, 7));
  assert(component.callbacks_.dispatch(239, 3));
  assert(first == 7 && second == 3);
  assert(!component.callbacks_.dispatch(0, 1));
  assert(!component.callbacks_.dispatch(1, 1));
  assert(!component.callbacks_.dispatch(240, 1));
}
