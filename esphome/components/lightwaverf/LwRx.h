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
  uint8_t lwrx_addpair(uint8_t *pairdata);

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

 protected:
  void lwrx_clearpairing_();

  // Return stats on pulse timings
  bool lwrx_getstats_(uint16_t *stats);

  // Enable collection of stats on pulse timings
  void lwrx_setstatsenable_(bool rx_stats_enable);

  // Set base address for EEPROM storage
  void lwrx_set_eepro_maddr_(int addr);

  // internal support functions
  bool rx_report_message_();
  int16_t rx_find_nibble_(uint8_t data);  // int
  void rx_addpairfrommsg_();
  void rx_paircommit_();
  void rx_remove_pair_(uint8_t *buf);
  int16_t rx_check_pairs_(uint8_t *buf, bool all_devices);  // int
  void restore_eeprom_pairing_();

  ISRInternalGPIOPin rx_pin_isr_;
  InternalGPIOPin *rx_pin_;
};

}  // namespace lightwaverf
}  // namespace esphome
