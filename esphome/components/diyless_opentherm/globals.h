#pragma once

#include "opentherm.h"
#include <Arduino.h>

namespace esphome {
namespace diyless_opentherm {

extern ihormelnyk::OpenTherm *openTherm;

ICACHE_RAM_ATTR void handleInterrupt();
void responseCallback(unsigned long response, ihormelnyk::OpenThermResponseStatus responseStatus);

} // namespace diyless_opentherm
} // namespace esphome
