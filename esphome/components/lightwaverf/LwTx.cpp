// LwTx.cpp
//
// LightwaveRF 434MHz tx interface for Arduino
//
// Author: Bob Tidey (robert@tideys.net)
#include "LwTx.h"
#include <cstring>
#include <core_esp8266_timer.cpp>

namespace esphome {
namespace lightwaverf {

static int EEPROMaddr = EEPROM_ADDR_DEFAULT;

static uint8_t tx_nibble[] = {0xF6, 0xEE, 0xED, 0xEB, 0xDE, 0xDD, 0xDB, 0xBE,
                              0xBD, 0xBB, 0xB7, 0x7E, 0x7D, 0x7B, 0x77, 0x6F};

#ifdef TX_PIN_DEFAULT
static int tx_pin = TX_PIN_DEFAULT;
#else
static int tx_pin = 3;
#endif

static const uint8_t tx_msglen = 10;  // the expected length of the message

// Transmit mode constants and variables
static uint8_t tx_repeats = 12;  // Number of repeats of message sent
static uint8_t txon = 1;
static uint8_t txoff = 0;
static bool tx_msg_active = false;  // set true to activate message sending
static bool tx_translate = true;    // Set false to send raw data

static uint8_t tx_buf[tx_msglen];  // the message buffer during reception
static uint8_t tx_repeat = 0;      // counter for repeats
static uint8_t tx_state = 0;
static uint16_t tx_gap_repeat = 0;  // unsigned int

// Use with low repeat counts
static uint8_t tx_gap_count = 33;    // Inter-message gap count (10.9 msec)
static unsigned long espPeriod = 0;  // Holds interrupt timer0 period
static unsigned long espNext = 0;    // Holds interrupt next count

// Gap multiplier byte is used to multiply gap if longer periods are needed for experimentation
// If gap is 255 (35msec) then this to give a max of 9 seconds
// Used with low repeat counts to find if device times out
static uint8_t tx_gap_multiplier = 0;  // Gap extension byte

static const uint8_t tx_state_idle = 0;
static const uint8_t tx_state_msgstart = 1;
static const uint8_t tx_state_bytestart = 2;
static const uint8_t tx_state_sendbyte = 3;
static const uint8_t tx_state_msgend = 4;
static const uint8_t tx_state_gapstart = 5;
static const uint8_t tx_state_gapend = 6;

static uint8_t tx_bit_mask = 0;   // bit mask in current byte
static uint8_t tx_num_bytes = 0;  // number of bytes sent

uint8_t tx_toggle_count = 3;

// These set the pulse durations in ticks. ESP uses 330uSec base tick, else use 140uSec
uint8_t tx_low_count = 3;    // total number of ticks in a low (990 uSec)
uint8_t tx_high_count = 2;   // total number of ticks in a high (660 uSec)
uint8_t tx_trail_count = 1;  // tick count to set line low (330 uSec)

/**
  Set translate mode
**/
void LwTx::lwtx_settranslate(bool txtranslate) { tx_translate = txtranslate; }

static void IRAM_ATTR isrTXtimer() {
  /*

  // Set low after toggle count interrupts
  tx_toggle_count--;
  if (tx_toggle_count == tx_trail_count) {
    digitalWrite(tx_pin->get_pin(), txoff);
    // digitalWrite(tx_pin, txoff);
  } else if (arg->tx_toggle_count == 0) {
    arg->tx_toggle_count = arg->tx_high_count;  // default high pulse duration
    switch (tx_state) {
      case tx_state_idle:
        if (tx_msg_active) {
          tx_repeat = 0;
          tx_state = tx_state_msgstart;
        }
        break;
      case tx_state_msgstart:
        arg->tx_pin.digital_write(txon);
        // digitalWrite(tx_pin, txon);
        tx_num_bytes = 0;
        tx_state = tx_state_bytestart;
        break;
      case tx_state_bytestart:
        arg->tx_pin.digital_write(txon);
        // digitalWrite(tx_pin, txon);
        tx_bit_mask = 0x80;
        tx_state = tx_state_sendbyte;
        break;
      case tx_state_sendbyte:
        if (tx_buf[tx_num_bytes] & tx_bit_mask) {
          arg->tx_pin.digital_write(txon);
          // digitalWrite(tx_pin, txon);
        } else {
          // toggle count for the 0 pulse
          arg->tx_toggle_count = arg->tx_low_count;
        }
        tx_bit_mask >>= 1;
        if (tx_bit_mask == 0) {
          tx_num_bytes++;
          if (tx_num_bytes >= tx_msglen) {
            tx_state = tx_state_msgend;
          } else {
            tx_state = tx_state_bytestart;
          }
        }
        break;
      case tx_state_msgend:
        arg->tx_pin.digital_write(txon);
        // digitalWrite(tx_pin, txon);
        tx_state = tx_state_gapstart;
        tx_gap_repeat = tx_gap_multiplier;
        break;
      case tx_state_gapstart:
        arg->tx_toggle_count = tx_gap_count;
        if (tx_gap_repeat == 0) {
          tx_state = tx_state_gapend;
        } else {
          tx_gap_repeat--;
        }
        break;
      case tx_state_gapend:
        tx_repeat++;
        if (tx_repeat >= tx_repeats) {
          // disable timer nterrupt
          arg->lw_timer_Stop();
          tx_msg_active = false;
          tx_state = tx_state_idle;
        } else {
          tx_state = tx_state_msgstart;
        }
        break;
    }
  }
#if ESP8266_TIMER == 0
  espNext += espPeriod;
  timer0_write(espNext);
#endif
*/
}

/**
  Check for send free
**/
bool LwTx::lwtx_free() { return !tx_msg_active; }

/**
  Send a LightwaveRF message (10 nibbles in bytes)
**/
void LwTx::lwtx_send(uint8_t *msg) {
  if (tx_translate) {
    for (uint8_t i = 0; i < tx_msglen; i++) {
      tx_buf[i] = tx_nibble[msg[i] & 0xF];
    }
  } else {
    memcpy(tx_buf, msg, tx_msglen);
  }
  this->lw_timer_Start();
  tx_msg_active = true;
}

/**
  Set 5 char address for future messages
**/
void LwTx::lwtx_setaddr(uint8_t *addr) {
  for (uint8_t i = 0; i < 5; i++) {
    tx_buf[i + 4] = tx_nibble[addr[i] & 0xF];
#if EEPROM_EN
    EEPROM.write(EEPROMaddr + i, tx_buf[i + 4]);
#endif
  }
}

/**
  Send a LightwaveRF message (10 nibbles in bytes)
**/
void LwTx::lwtx_cmd(uint8_t command, uint8_t parameter, uint8_t room, uint8_t device) {
  // enable timer 2 interrupts
  tx_buf[0] = tx_nibble[parameter >> 4];
  tx_buf[1] = tx_nibble[parameter & 0xF];
  tx_buf[2] = tx_nibble[device & 0xF];
  tx_buf[3] = tx_nibble[command & 0xF];
  tx_buf[9] = tx_nibble[room & 0xF];
  this->lw_timer_Start();
  tx_msg_active = true;
}

/**
  Set things up to transmit LightWaveRF 434Mhz messages
**/
void LwTx::lwtx_setup(InternalGPIOPin *pin, uint8_t repeats, uint8_t invert, int period) {
#if EEPROM_EN
  for (int i = 0; i < 5; i++) {
    this->tx_buf[i + 4] = EEPROM.read(this->EEPROMaddr + i);
  }
#endif

  pin->setup();
  tx_pin = pin;
  // tx_pin_isr = pin->to_isr();

  digitalWrite(tx_pin->get_pin(), txoff);

  if (repeats > 0 && repeats < 40) {
    tx_repeats = repeats;
  }
  if (invert != 0) {
    txon = 0;
    txoff = 1;
  } else {
    txon = 1;
    txoff = 0;
  }

  int period1;
  if (period > 32 && period < 1000) {
    period1 = period;
  } else {
    // default 330 uSec
    period1 = 330;
  }
  espPeriod = 5 * period1;
  timer1_isr_init();
}

void LwTx::lwtx_setTickCounts(uint8_t lowCount, uint8_t highCount, uint8_t trailCount, uint8_t gapCount) {
  tx_low_count = lowCount;
  tx_high_count = highCount;
  tx_trail_count = trailCount;
  tx_gap_count = gapCount;
}

void LwTx::lwtx_setGapMultiplier(uint8_t gapMultiplier) { tx_gap_multiplier = gapMultiplier; }

/**
  Set EEPROMAddr
**/
void LwTx::lwtx_setEEPROMaddr(int addr) { EEPROMaddr = addr; }

void LwTx::lw_timer_Start() {
  noInterrupts();
  timer1_attachInterrupt(isrTXtimer);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);
  timer1_write(espPeriod);
  interrupts();
}

void LwTx::lw_timer_Stop() {
  noInterrupts();
  timer1_disable();
  timer1_detachInterrupt();
  interrupts();
}

}  // namespace lightwaverf
}  // namespace esphome
