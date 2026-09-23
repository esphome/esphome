#include "actuator.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

#include <strings.h>

namespace esphome::actuator {

// Actuator operation strings indexed by ActuatorOperation enum (0-2): IDLE, OPENING, CLOSING, plus UNKNOWN
PROGMEM_STRING_TABLE(ActuatorOperationStrings, "IDLE", "OPENING", "CLOSING", "UNKNOWN");

const LogString *actuator_operation_to_str(ActuatorOperation op) {
  return ActuatorOperationStrings::get_log_str(static_cast<uint8_t>(op), ActuatorOperationStrings::LAST_INDEX);
}

//
// ActuatorCallBase
//

bool ActuatorCallBase::set_command_(const char *command) {
  if (ESPHOME_strcasecmp_P(command, ESPHOME_PSTR("OPEN")) == 0) {
    this->set_command_open();
  } else if (ESPHOME_strcasecmp_P(command, ESPHOME_PSTR("CLOSE")) == 0) {
    this->set_command_close();
  } else if (ESPHOME_strcasecmp_P(command, ESPHOME_PSTR("STOP")) == 0) {
    this->set_command_stop();
  } else if (ESPHOME_strcasecmp_P(command, ESPHOME_PSTR("TOGGLE")) == 0) {
    this->set_command_toggle();
  } else {
    return false;
  }
  return true;
}

//
// ActuatorBase
//

bool ActuatorBase::is_fully_open() const { return this->position == ACTUATOR_OPEN; }
bool ActuatorBase::is_fully_closed() const { return this->position == ACTUATOR_CLOSED; }

}  // namespace esphome::actuator
