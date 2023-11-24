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

// Include basic library header and set default TX pin
static const uint8_t TX_PIN_DEFAULT = 13;

class LwTx {
 public:
  // Sets up basic parameters must be called at least once
  void lwtx_setup(InternalGPIOPin *pin, uint8_t repeats, bool inverted, int u_sec);

  // Allows changing basic tick counts from their defaults
  void lwtx_set_tick_counts(uint8_t low_count, uint8_t high_count, uint8_t trail_count, uint8_t gap_count);

  // Allws multiplying the gap period for creating very large gaps
  void lwtx_set_gap_multiplier(uint8_t gap_multiplier);

  // determines whether incoming data or should be translated from nibble data
  void lwtx_settranslate(bool txtranslate);

  // Checks whether tx is free to accept a new message
  bool lwtx_free();

  // Basic send of new 10 char message, not normally needed if setaddr and cmd are used.
  void lwtx_send(const std::vector<uint8_t> &msg);

  // Sets up 5 char address which will be used to form messages for lwtx_cmd
  void lwtx_setaddr(const uint8_t *addr);

  // Send Command
  void lwtx_cmd(uint8_t command, uint8_t parameter, uint8_t room, uint8_t device);

  // Allows changing basic tick counts from their defaults
  void lw_timer_start();

  // Allws multiplying the gap period for creating very large gaps
  void lw_timer_stop();

  // These set the pulse durationlws in ticks. ESP uses 330uSec base tick, else use 140uSec
  uint8_t tx_low_count = 3;    // total number of ticks in a low (990 uSec)
  uint8_t tx_high_count = 2;   // total number of ticks in a high (660 uSec)
  uint8_t tx_trail_count = 1;  // tick count to set line low (330 uSec)

  uint8_t tx_toggle_count = 3;

  static const uint8_t TX_MSGLEN = 10;  // the expected length of the message

  // Transmit mode constants and variables
  uint8_t tx_repeats = 12;  // Number of repeats of message sent
  uint8_t txon = 1;
  uint8_t txoff = 0;
  bool tx_msg_active = false;  // set true to activate message sending
  bool tx_translate = true;    // Set false to send raw data

  uint8_t tx_buf[TX_MSGLEN];  // the message buffer during reception
  uint8_t tx_repeat = 0;      // counter for repeats
  uint8_t tx_state = 0;
  uint16_t tx_gap_repeat = 0;  // unsigned int

  // Use with low repeat counts
  uint8_t tx_gap_count = 33;  // Inter-message gap count (10.9 msec)
  uint32_t espPeriod = 0;     // Holds interrupt timer0 period
  uint32_t espNext = 0;       // Holds interrupt next count

  // Gap multiplier byte is used to multiply gap if longer periods are needed for experimentation
  // If gap is 255 (35msec) then this to give a max of 9 seconds
  // Used with low repeat counts to find if device times out
  uint8_t tx_gap_multiplier = 0;  // Gap extension byte

  uint8_t tx_bit_mask = 0;   // bit mask in current byte
  uint8_t tx_num_bytes = 0;  // number of bytes sent

  InternalGPIOPin *tx_pin;

 protected:
  uint32_t duty_on_;
  uint32_t duty_off_;
};

}  // namespace lightwaverf
}  // namespace esphome
