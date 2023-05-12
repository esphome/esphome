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

static const uint8_t rx_stat_high_ave = 0;
static const uint8_t rx_stat_high_max = 1;
static const uint8_t rx_stat_high_min = 2;
static const uint8_t rx_stat_low0_ave = 3;
static const uint8_t rx_stat_low0_max = 4;
static const uint8_t rx_stat_low0_min = 5;
static const uint8_t rx_stat_low1_ave = 6;
static const uint8_t rx_stat_low1_max = 7;
static const uint8_t rx_stat_low1_min = 8;
static const uint8_t rx_stat_count = 9;

// sets maximum number of pairings which can be held
static const uint8_t rx_maxpairs = 10;

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
  void lwrx_setPairMode(bool pairEnforce, bool pairBaseOnly);

  // Returns time from last packet received in msec
  //  Can be used to determine if Rx may be still receiving repeats
  unsigned long lwrx_packetinterval();

  static void rx_process_bits(LwRx *arg);

 protected:
  void lwrx_clearpairing();

  // Return stats on pulse timings
  bool lwrx_getstats(uint16_t *stats);

  // Enable collection of stats on pulse timings
  void lwrx_setstatsenable(bool rx_stats_enable);

  // Set base address for EEPROM storage
  void lwrx_setEEPROMaddr(int addr);

  // internal support functions
  bool rx_reportMessage();
  int16_t rx_findNibble(uint8_t data);  // int
  void rx_addpairfrommsg();
  void rx_paircommit();
  void rx_removePair(uint8_t *buf);
  int16_t rx_checkPairs(uint8_t *buf, bool allDevices);  // int
  void restoreEEPROMPairing();

  ISRInternalGPIOPin rx_pin_isr;
  InternalGPIOPin *rx_pin;
};

}  // namespace lightwaverf
}  // namespace esphome
