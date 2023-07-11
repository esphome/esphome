#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace lightwaverf {

// LwRx.h
//
// LightwaveRF 434MHz receiver for Arduino
//
// Author: Bob Tidey (robert@tideys.net)

static const uint8_t RX_STAT_HIGH_AVE = 0;
static const uint8_t RX_STAT_HIGH_MAX = 1;
static const uint8_t RX_STAT_HIGH_MIN = 2;
static const uint8_t RX_STAT_LOW0_AVE = 3;
static const uint8_t RX_STAT_LOW0_MAX = 4;
static const uint8_t RX_STAT_LOW0_MIN = 5;
static const uint8_t RX_STAT_LOW1_AVE = 6;
static const uint8_t RX_STAT_LOW1_MAX = 7;
static const uint8_t RX_STAT_LOW1_MIN = 8;
static const uint8_t RX_STAT_COUNT = 9;

// sets maximum number of pairings which can be held
static const uint8_t RX_MAXPAIRS = 10;

static const uint8_t RX_NIBBLE[] = {0xF6, 0xEE, 0xED, 0xEB, 0xDE, 0xDD, 0xDB, 0xBE,
                                    0xBD, 0xBB, 0xB7, 0x7E, 0x7D, 0x7B, 0x77, 0x6F};
static const uint8_t RX_CMD_OFF = 0xF6;      // raw 0
static const uint8_t RX_CMD_ON = 0xEE;       // raw 1
static const uint8_t RX_CMD_MOOD = 0xED;     // raw 2
static const uint8_t RX_PAR0_ALLOFF = 0x7D;  // param 192-255 all off (12 in msb)
static const uint8_t RX_DEV_15 = 0x6F;       // device 15

static const uint8_t RX_MSGLEN = 10;  // expected length of rx message

static const uint8_t RX_STATE_IDLE = 0;
static const uint8_t RX_STATE_MSGSTARTFOUND = 1;
static const uint8_t RX_STATE_BYTESTARTFOUND = 2;
static const uint8_t RX_STATE_GETBYTE = 3;

// Gather stats for pulse widths (ave is x 16)
static const uint16_t LWRX_STATSDFLT[RX_STAT_COUNT] = {5000, 0, 5000, 20000, 0, 2500, 4000, 0, 500};  // usigned int

class LwRx {
 public:
  // Seup must be called once, set up pin used to receive data
  void lwrx_setup(InternalGPIOPin *pin);

  // Set translate to determine whether translating from nibbles to bytes in message
  // Translate off only applies to 10char message returns
  void lwrx_settranslate(bool translate);

  // Check to see whether message available
  bool lwrx_message();

  // Get a message, len controls format (2 cmd+param, 4 cmd+param+room+device),10 full message
  bool lwrx_getmessage(uint8_t *buf, uint8_t len);

  // Setup repeat filter
  void lwrx_setfilter(uint8_t repeats, uint8_t timeout);

  // Add pair, if no pairing set then all messages are received, returns number of pairs
  uint8_t lwrx_addpair(const uint8_t *pairdata);

  // Get pair data into buffer  for the pairnumber. Returns current paircount
  // Use pairnumber 255 to just get current paircount
  uint8_t lwrx_getpair(uint8_t *pairdata, uint8_t pairnumber);

  // Make a pair from next message received within timeout 100mSec
  // This call returns immediately whilst message checking continues
  void lwrx_makepair(uint8_t timeout);

  // Set pair mode controls
  void lwrx_set_pair_mode(bool pair_enforce, bool pair_base_only);

  // Returns time from last packet received in msec
  //  Can be used to determine if Rx may be still receiving repeats
  uint32_t lwrx_packetinterval();

  static void rx_process_bits(LwRx *arg);

  // Pairing data
  uint8_t rx_paircount = 0;
  uint8_t rx_pairs[RX_MAXPAIRS][8];
  // set false to responds to all messages if no pairs set up
  bool rx_pairEnforce = false;
  // set false to use Address, Room and Device in pairs, true just the Address part
  bool rx_pairBaseOnly = false;

  uint8_t rx_pairtimeout = 0;  // 100msec units

  // Repeat filters
  uint8_t rx_repeats = 2;  // msg must be repeated at least this number of times
  uint8_t rx_repeatcount = 0;
  uint8_t rx_timeout = 20;        // reset repeat window after this in 100mSecs
  uint32_t rx_prevpkttime = 0;    // last packet time in milliseconds
  uint32_t rx_pairstarttime = 0;  // last msg time in milliseconds

  // Receive mode constants and variables
  uint8_t rx_msg[RX_MSGLEN];  // raw message received
  uint8_t rx_buf[RX_MSGLEN];  // message buffer during reception

  uint32_t rx_prev;  // time of previous interrupt in microseconds

  bool rx_msgcomplete = false;  // set high when message available
  bool rx_translate = true;     // Set false to get raw data

  uint8_t rx_state = 0;

  uint8_t rx_num_bits = 0;   // number of bits in the current uint8_t
  uint8_t rx_num_bytes = 0;  // number of bytes received

  uint16_t lwrx_stats[RX_STAT_COUNT];  // unsigned int

  bool lwrx_stats_enable = true;

 protected:
  void lwrx_clearpairing_();

  // Return stats on pulse timings
  bool lwrx_getstats_(uint16_t *stats);

  // Enable collection of stats on pulse timings
  void lwrx_setstatsenable_(bool rx_stats_enable);

  // internal support functions
  bool rx_report_message_();
  int16_t rx_find_nibble_(uint8_t data);  // int
  void rx_addpairfrommsg_();
  void rx_paircommit_();
  void rx_remove_pair_(uint8_t *buf);
  int16_t rx_check_pairs_(const uint8_t *buf, bool all_devices);  // int

  ISRInternalGPIOPin rx_pin_isr_;
  InternalGPIOPin *rx_pin_;
};

}  // namespace lightwaverf
}  // namespace esphome
