// LwRx.cpp
//
// LightwaveRF 434MHz receiver interface for Arduino
//
// Author: Bob Tidey (robert@tideys.net)

#ifdef USE_ESP8266

#include "LwRx.h"
#include <cstring>

namespace esphome {
namespace lightwaverf {

/**
  Pin change interrupt routine that identifies 1 and 0 LightwaveRF bits
  and constructs a message when a valid packet of data is received.
**/

void IRAM_ATTR LwRx::rx_process_bits(LwRx *args) {
  uint8_t event = args->rx_pin_isr_.digital_read();  // start setting event to the current value
  uint32_t curr = micros();                          // the current time in microseconds

  uint16_t dur = (curr - args->rx_prev);  // unsigned int
  args->rx_prev = curr;
  // set event based on input and duration of previous pulse
  if (dur < 120) {         // 120 very short
  } else if (dur < 500) {  // normal short pulse
    event += 2;
  } else if (dur < 2000) {  // normal long pulse
    event += 4;
  } else if (dur > 5000) {  // gap between messages
    event += 6;
  } else {      // 2000 > 5000
    event = 8;  // illegal gap
  }
  // state machine transitions
  switch (args->rx_state) {
    case RX_STATE_IDLE:
      switch (event) {
        case 7:  // 1 after a message gap
          args->rx_state = RX_STATE_MSGSTARTFOUND;
          break;
      }
      break;
    case RX_STATE_MSGSTARTFOUND:
      switch (event) {
        case 2:  // 0 160->500
                 // nothing to do wait for next positive edge
          break;
        case 3:  // 1 160->500
          args->rx_num_bytes = 0;
          args->rx_state = RX_STATE_BYTESTARTFOUND;
          break;
        default:
          // not good start again
          args->rx_state = RX_STATE_IDLE;
          break;
      }
      break;
    case RX_STATE_BYTESTARTFOUND:
      switch (event) {
        case 2:  // 0 160->500
          // nothing to do wait for next positive edge
          break;
        case 3:  // 1 160->500
          args->rx_state = RX_STATE_GETBYTE;
          args->rx_num_bits = 0;
          break;
        case 5:  // 0 500->1500
          args->rx_state = RX_STATE_GETBYTE;
          // Starts with 0 so put this into uint8_t
          args->rx_num_bits = 1;
          args->rx_buf[args->rx_num_bytes] = 0;
          break;
        default:
          // not good start again
          args->rx_state = RX_STATE_IDLE;
          break;
      }
      break;
    case RX_STATE_GETBYTE:
      switch (event) {
        case 2:  // 0 160->500
          // nothing to do wait for next positive edge but do stats
          if (args->lwrx_stats_enable) {
            args->lwrx_stats[RX_STAT_HIGH_MAX] = std::max(args->lwrx_stats[RX_STAT_HIGH_MAX], dur);
            args->lwrx_stats[RX_STAT_HIGH_MIN] = std::min(args->lwrx_stats[RX_STAT_HIGH_MIN], dur);
            args->lwrx_stats[RX_STAT_HIGH_AVE] =
                args->lwrx_stats[RX_STAT_HIGH_AVE] - (args->lwrx_stats[RX_STAT_HIGH_AVE] >> 4) + dur;
          }
          break;
        case 3:  // 1 160->500
          // a single 1
          args->rx_buf[args->rx_num_bytes] = args->rx_buf[args->rx_num_bytes] << 1 | 1;
          args->rx_num_bits++;
          if (args->lwrx_stats_enable) {
            args->lwrx_stats[RX_STAT_LOW1_MAX] = std::max(args->lwrx_stats[RX_STAT_LOW1_MAX], dur);
            args->lwrx_stats[RX_STAT_LOW1_MIN] = std::min(args->lwrx_stats[RX_STAT_LOW1_MIN], dur);
            args->lwrx_stats[RX_STAT_LOW1_AVE] =
                args->lwrx_stats[RX_STAT_LOW1_AVE] - (args->lwrx_stats[RX_STAT_LOW1_AVE] >> 4) + dur;
          }
          break;
        case 5:  // 1 500->1500
          // a 1 followed by a 0
          args->rx_buf[args->rx_num_bytes] = args->rx_buf[args->rx_num_bytes] << 2 | 2;
          args->rx_num_bits++;
          args->rx_num_bits++;
          if (args->lwrx_stats_enable) {
            args->lwrx_stats[RX_STAT_LOW0_MAX] = std::max(args->lwrx_stats[RX_STAT_LOW0_MAX], dur);
            args->lwrx_stats[RX_STAT_LOW0_MIN] = std::min(args->lwrx_stats[RX_STAT_LOW0_MIN], dur);
            args->lwrx_stats[RX_STAT_LOW0_AVE] =
                args->lwrx_stats[RX_STAT_LOW0_AVE] - (args->lwrx_stats[RX_STAT_LOW0_AVE] >> 4) + dur;
          }
          break;
        default:
          // not good start again
          args->rx_state = RX_STATE_IDLE;
          break;
      }
      if (args->rx_num_bits >= 8) {
        args->rx_num_bytes++;
        args->rx_num_bits = 0;
        if (args->rx_num_bytes >= RX_MSGLEN) {
          uint32_t curr_millis = millis();
          if (args->rx_repeats > 0) {
            if ((curr_millis - args->rx_prevpkttime) / 100 > args->rx_timeout) {
              args->rx_repeatcount = 1;
            } else {
              // Test message same as last one
              int16_t i = RX_MSGLEN;  // int
              do {
                i--;
              } while ((i >= 0) && (args->rx_msg[i] == args->rx_buf[i]));
              if (i < 0) {
                args->rx_repeatcount++;
              } else {
                args->rx_repeatcount = 1;
              }
            }
          } else {
            args->rx_repeatcount = 0;
          }
          args->rx_prevpkttime = curr_millis;
          // If last message hasn't been read it gets overwritten
          memcpy(args->rx_msg, args->rx_buf, RX_MSGLEN);
          if (args->rx_repeats == 0 || args->rx_repeatcount == args->rx_repeats) {
            if (args->rx_pairtimeout != 0) {
              if ((curr_millis - args->rx_pairstarttime) / 100 <= args->rx_pairtimeout) {
                if (args->rx_msg[3] == RX_CMD_ON) {
                  args->rx_addpairfrommsg_();
                } else if (args->rx_msg[3] == RX_CMD_OFF) {
                  args->rx_remove_pair_(&args->rx_msg[2]);
                }
              }
            }
            if (args->rx_report_message_()) {
              args->rx_msgcomplete = true;
            }
            args->rx_pairtimeout = 0;
          }
          // And cycle round for next one
          args->rx_state = RX_STATE_IDLE;
        } else {
          args->rx_state = RX_STATE_BYTESTARTFOUND;
        }
      }
      break;
  }
}

/**
  Test if a message has arrived
**/
bool LwRx::lwrx_message() { return (this->rx_msgcomplete); }

/**
  Set translate mode
**/
void LwRx::lwrx_settranslate(bool rxtranslate) { this->rx_translate = rxtranslate; }
/**
  Transfer a message to user buffer
**/
bool LwRx::lwrx_getmessage(uint8_t *buf, uint8_t len) {
  bool ret = true;
  int16_t j = 0;  // int
  if (this->rx_msgcomplete && len <= RX_MSGLEN) {
    for (uint8_t i = 0; ret && i < RX_MSGLEN; i++) {
      if (this->rx_translate || (len != RX_MSGLEN)) {
        j = this->rx_find_nibble_(this->rx_msg[i]);
        if (j < 0)
          ret = false;
      } else {
        j = this->rx_msg[i];
      }
      switch (len) {
        case 4:
          if (i == 9)
            buf[2] = j;
          if (i == 2)
            buf[3] = j;
        case 2:
          if (i == 3)
            buf[0] = j;
          if (i == 0)
            buf[1] = j << 4;
          if (i == 1)
            buf[1] += j;
          break;
        case 10:
          buf[i] = j;
          break;
      }
    }
    this->rx_msgcomplete = false;
  } else {
    ret = false;
  }
  return ret;
}

/**
  Return time in milliseconds since last packet received
**/
uint32_t LwRx::lwrx_packetinterval() { return millis() - this->rx_prevpkttime; }

/**
  Set up repeat filtering of received messages
**/
void LwRx::lwrx_setfilter(uint8_t repeats, uint8_t timeout) {
  this->rx_repeats = repeats;
  this->rx_timeout = timeout;
}

/**
  Add a pair to filter received messages
  pairdata is device,dummy,5*addr,room
  pairdata is held in translated form to make comparisons quicker
**/
uint8_t LwRx::lwrx_addpair(const uint8_t *pairdata) {
  if (this->rx_paircount < RX_MAXPAIRS) {
    for (uint8_t i = 0; i < 8; i++) {
      this->rx_pairs[rx_paircount][i] = RX_NIBBLE[pairdata[i]];
    }
    this->rx_paircommit_();
  }
  return this->rx_paircount;
}

/**
  Make a pair from next message successfully received
**/
void LwRx::lwrx_makepair(uint8_t timeout) {
  this->rx_pairtimeout = timeout;
  this->rx_pairstarttime = millis();
}

/**
  Get pair data (translated back to nibble form
**/
uint8_t LwRx::lwrx_getpair(uint8_t *pairdata, uint8_t pairnumber) {
  if (pairnumber < this->rx_paircount) {
    int16_t j;  // int
    for (uint8_t i = 0; i < 8; i++) {
      j = this->rx_find_nibble_(this->rx_pairs[pairnumber][i]);
      if (j >= 0)
        pairdata[i] = j;
    }
  }
  return this->rx_paircount;
}

/**
  Clear all pairing
**/
void LwRx::lwrx_clearpairing_() { rx_paircount = 0; }

/**
  Return stats on high and low pulses
**/
bool LwRx::lwrx_getstats_(uint16_t *stats) {  // unsigned int
  if (this->lwrx_stats_enable) {
    memcpy(stats, this->lwrx_stats, 2 * RX_STAT_COUNT);
    return true;
  } else {
    return false;
  }
}

/**
  Set stats mode
**/
void LwRx::lwrx_setstatsenable_(bool rx_stats_enable) {
  this->lwrx_stats_enable = rx_stats_enable;
  if (!this->lwrx_stats_enable) {
    // clear down stats when disabling
    memcpy(this->lwrx_stats, LWRX_STATSDFLT, sizeof(LWRX_STATSDFLT));
  }
}
/**
  Set pairs behaviour
**/
void LwRx::lwrx_set_pair_mode(bool pair_enforce, bool pair_base_only) {
  this->rx_pairEnforce = pair_enforce;
  this->rx_pairBaseOnly = pair_base_only;
}

/**
  Set things up to receive LightWaveRF 434Mhz messages
  pin must be 2 or 3 to trigger interrupts
  !!! For Spark, any pin will work
**/
void LwRx::lwrx_setup(InternalGPIOPin *pin) {
  // rx_pin = pin;
  pin->setup();
  this->rx_pin_isr_ = pin->to_isr();
  pin->attach_interrupt(&LwRx::rx_process_bits, this, gpio::INTERRUPT_ANY_EDGE);

  memcpy(this->lwrx_stats, LWRX_STATSDFLT, sizeof(LWRX_STATSDFLT));
}

/**
  Check a message to see if it should be reported under pairing / mood / all off rules
  returns -1 if none found
**/
bool LwRx::rx_report_message_() {
  if (this->rx_pairEnforce && this->rx_paircount == 0) {
    return false;
  } else {
    bool all_devices;
    // True if mood to device 15 or Off cmd with Allof paramater
    all_devices = ((this->rx_msg[3] == RX_CMD_MOOD && this->rx_msg[2] == RX_DEV_15) ||
                   (this->rx_msg[3] == RX_CMD_OFF && this->rx_msg[0] == RX_PAR0_ALLOFF));
    return (rx_check_pairs_(&this->rx_msg[2], all_devices) != -1);
  }
}
/**
  Find nibble from byte
  returns -1 if none found
**/
int16_t LwRx::rx_find_nibble_(uint8_t data) {  // int
  int16_t i = 15;                              // int
  do {
    if (RX_NIBBLE[i] == data)
      break;
    i--;
  } while (i >= 0);
  return i;
}

/**
  add pair from message buffer
**/
void LwRx::rx_addpairfrommsg_() {
  if (this->rx_paircount < RX_MAXPAIRS) {
    memcpy(this->rx_pairs[this->rx_paircount], &this->rx_msg[2], 8);
    this->rx_paircommit_();
  }
}

/**
  check and commit pair
**/
void LwRx::rx_paircommit_() {
  if (this->rx_paircount == 0 || this->rx_check_pairs_(this->rx_pairs[this->rx_paircount], false) < 0) {
    this->rx_paircount++;
  }
}

/**
  Check to see if message matches one of the pairs
    if mode is pairBase only then ignore device and room
    if allDevices is true then ignore the device number
  Returns matching pair number, -1 if not found, -2 if no pairs defined
**/
int16_t LwRx::rx_check_pairs_(const uint8_t *buf, bool all_devices) {  // int
  if (this->rx_paircount == 0) {
    return -2;
  } else {
    int16_t pair = this->rx_paircount;  // int
    int16_t j = -1;                     // int
    int16_t jstart, jend;               // int
    if (this->rx_pairBaseOnly) {
      // skip room(8) and dev/cmd (0,1)
      jstart = 7;
      jend = 2;
    } else {
      // include room in comparison
      jstart = 8;
      // skip device comparison if allDevices true
      jend = (all_devices) ? 2 : 0;
    }
    while (pair > 0 && j < 0) {
      pair--;
      j = jstart;
      while (j > jend) {
        j--;
        if (j != 1) {
          if (this->rx_pairs[pair][j] != buf[j]) {
            j = -1;
          }
        }
      }
    }
    return (j >= 0) ? pair : -1;
  }
}

/**
  Remove an existing pair matching the buffer
**/
void LwRx::rx_remove_pair_(uint8_t *buf) {
  int16_t pair = this->rx_check_pairs_(buf, false);  // int
  if (pair >= 0) {
    while (pair < this->rx_paircount - 1) {
      for (uint8_t j = 0; j < 8; j++) {
        this->rx_pairs[pair][j] = this->rx_pairs[pair + 1][j];
      }
      pair++;
    }
    this->rx_paircount--;
  }
}

}  // namespace lightwaverf
}  // namespace esphome
#endif
