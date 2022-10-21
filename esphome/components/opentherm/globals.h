#pragma once

#include "opentherm.h"
#include <Arduino.h>

namespace esphome {
namespace diyless_opentherm {

extern ihormelnyk::OpenTherm *openTherm;

IRAM_ATTR void handle_interrupt();
void response_callback(unsigned long response, ihormelnyk::OpenThermResponseStatus response_status);

}  // namespace diyless_opentherm
}  // namespace esphome
