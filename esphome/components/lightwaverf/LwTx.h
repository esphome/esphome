#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace lightwaverf {

// LxTx.h
//
// LightwaveRF 434MHz tx interface for Arduino
//
// Author: Bob Tidey (robert@tideys.net)

#define ESP8266 1

// Choose whether to include EEPROM support, comment or set to 0 to disable, 1 use with library support, 2 use with
// native support
#define EEPROM_EN 0

// Include basic library header and set default TX pin
#define TX_PIN_DEFAULT 13

// Include EEPROM if required to include storing device paramters in EEPROM
#if EEPROM_EN == 1
#include <EEPROM.h>
#endif
// define default EEPROMaddr to location to store message addr
#define EEPROM_ADDR_DEFAULT 0

class LwTx {
 public:
  // Sets up basic parameters must be called at least once
  void lwtx_setup(InternalGPIOPin *pin, uint8_t repeats, uint8_t invert, int uSec);

  // Allows changing basic tick counts from their defaults
  void lwtx_setTickCounts(uint8_t lowCount, uint8_t highCount, uint8_t trailCount, uint8_t gapCount);

  // Allws multiplying the gap period for creating very large gaps
  void lwtx_setGapMultiplier(uint8_t gapMultiplier);

  // determines whether incoming data or should be translated from nibble data
  void lwtx_settranslate(bool txtranslate);

  // Checks whether tx is free to accept a new message
  bool lwtx_free();

  // Basic send of new 10 char message, not normally needed if setaddr and cmd are used.
  void lwtx_send(uint8_t *msg);

  // Sets up 5 char address which will be used to form messages for lwtx_cmd
  void lwtx_setaddr(uint8_t *addr);

  // Send Command
  void lwtx_cmd(uint8_t command, uint8_t parameter, uint8_t room, uint8_t device);

  // Set base address for EEPROM storage
  void lwtx_setEEPROMaddr(int addr);

 protected:
  // Allows changing basic tick counts from their defaults
  void lw_timer_Start();

  // Allws multiplying the gap period for creating very large gaps
  void lw_timer_Stop();

  InternalGPIOPin *tx_pin;

  uint32_t duty_on;
  uint32_t duty_off;
};

}  // namespace lightwaverf
}  // namespace esphome