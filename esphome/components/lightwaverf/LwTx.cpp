// LwTx.cpp
//
// LightwaveRF 434MHz tx interface for Arduino
//
// Author: Bob Tidey (robert@tideys.net)
#ifdef USE_ESP8266

#include "LwTx.h"
#include <cstring>
#include <Arduino.h>
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace lightwaverf {

static const uint8_t TX_NIBBLE[] = {0xF6, 0xEE, 0xED, 0xEB, 0xDE, 0xDD, 0xDB, 0xBE,
                                    0xBD, 0xBB, 0xB7, 0x7E, 0x7D, 0x7B, 0x77, 0x6F};

static const uint8_t TX_STATE_IDLE = 0;
static const uint8_t TX_STATE_MSGSTART = 1;
static const uint8_t TX_STATE_BYTESTART = 2;
static const uint8_t TX_STATE_SENDBYTE = 3;
static const uint8_t TX_STATE_MSGEND = 4;
static const uint8_t TX_STATE_GAPSTART = 5;
static const uint8_t TX_STATE_GAPEND = 6;
/**
  Set translate mode
**/
void LwTx::lwtx_settranslate(bool txtranslate) { tx_translate = txtranslate; }

static void IRAM_ATTR isr_t_xtimer(LwTx *arg) {
  // Set low after toggle count interrupts
  arg->tx_toggle_count--;
  if (arg->tx_toggle_count == arg->tx_trail_count) {
    // ESP_LOGD("lightwaverf.sensor", "timer")
    arg->tx_pin->digital_write(arg->txoff);
  } else if (arg->tx_toggle_count == 0) {
    arg->tx_toggle_count = arg->tx_high_count;  // default high pulse duration
    switch (arg->tx_state) {
      case TX_STATE_IDLE:
        if (arg->tx_msg_active) {
          arg->tx_repeat = 0;
          arg->tx_state = TX_STATE_MSGSTART;
        }
        break;
      case TX_STATE_MSGSTART:
        arg->tx_pin->digital_write(arg->txon);
        arg->tx_num_bytes = 0;
        arg->tx_state = TX_STATE_BYTESTART;
        break;
      case TX_STATE_BYTESTART:
        arg->tx_pin->digital_write(arg->txon);
        arg->tx_bit_mask = 0x80;
        arg->tx_state = TX_STATE_SENDBYTE;
        break;
      case TX_STATE_SENDBYTE:
        if (arg->tx_buf[arg->tx_num_bytes] & arg->tx_bit_mask) {
          arg->tx_pin->digital_write(arg->txon);
        } else {
          // toggle count for the 0 pulse
          arg->tx_toggle_count = arg->tx_low_count;
        }
        arg->tx_bit_mask >>= 1;
        if (arg->tx_bit_mask == 0) {
          arg->tx_num_bytes++;
          if (arg->tx_num_bytes >= esphome::lightwaverf::LwTx::TX_MSGLEN) {
            arg->tx_state = TX_STATE_MSGEND;
          } else {
            arg->tx_state = TX_STATE_BYTESTART;
          }
        }
        break;
      case TX_STATE_MSGEND:
        arg->tx_pin->digital_write(arg->txon);
        arg->tx_state = TX_STATE_GAPSTART;
        arg->tx_gap_repeat = arg->tx_gap_multiplier;
        break;
      case TX_STATE_GAPSTART:
        arg->tx_toggle_count = arg->tx_gap_count;
        if (arg->tx_gap_repeat == 0) {
          arg->tx_state = TX_STATE_GAPEND;
        } else {
          arg->tx_gap_repeat--;
        }
        break;
      case TX_STATE_GAPEND:
        arg->tx_repeat++;
        if (arg->tx_repeat >= arg->tx_repeats) {
          // disable timer nterrupt
          arg->lw_timer_stop();
          arg->tx_msg_active = false;
          arg->tx_state = TX_STATE_IDLE;
        } else {
          arg->tx_state = TX_STATE_MSGSTART;
        }
        break;
    }
  }
}

/**
  Check for send free
**/
bool LwTx::lwtx_free() { return !this->tx_msg_active; }

/**
  Send a LightwaveRF message (10 nibbles in bytes)
**/
void LwTx::lwtx_send(const std::vector<uint8_t> &msg) {
  if (this->tx_translate) {
    for (uint8_t i = 0; i < TX_MSGLEN; i++) {
      this->tx_buf[i] = TX_NIBBLE[msg[i] & 0xF];
      ESP_LOGD("lightwaverf.sensor", "%x ", msg[i]);
    }
  } else {
    // memcpy(tx_buf, msg, tx_msglen);
  }
  this->lw_timer_start();
  this->tx_msg_active = true;
}

/**
  Set 5 char address for future messages
**/
void LwTx::lwtx_setaddr(const uint8_t *addr) {
  for (uint8_t i = 0; i < 5; i++) {
    this->tx_buf[i + 4] = TX_NIBBLE[addr[i] & 0xF];
  }
}

/**
  Send a LightwaveRF message (10 nibbles in bytes)
**/
void LwTx::lwtx_cmd(uint8_t command, uint8_t parameter, uint8_t room, uint8_t device) {
  // enable timer 2 interrupts
  this->tx_buf[0] = TX_NIBBLE[parameter >> 4];
  this->tx_buf[1] = TX_NIBBLE[parameter & 0xF];
  this->tx_buf[2] = TX_NIBBLE[device & 0xF];
  this->tx_buf[3] = TX_NIBBLE[command & 0xF];
  this->tx_buf[9] = TX_NIBBLE[room & 0xF];
  this->lw_timer_start();
  this->tx_msg_active = true;
}

/**
  Set things up to transmit LightWaveRF 434Mhz messages
**/
void LwTx::lwtx_setup(InternalGPIOPin *pin, uint8_t repeats, bool inverted, int u_sec) {
  pin->setup();
  tx_pin = pin;

  tx_pin->pin_mode(gpio::FLAG_OUTPUT);
  tx_pin->digital_write(txoff);

  if (repeats > 0 && repeats < 40) {
    this->tx_repeats = repeats;
  }
  if (inverted) {
    this->txon = 0;
    this->txoff = 1;
  } else {
    this->txon = 1;
    this->txoff = 0;
  }

  int period1 = 330;
  /*
  if (period > 32 && period < 1000) {
    period1 = period;
  } else {
    // default 330 uSec
    period1 = 330;
  }*/
  this->espPeriod = 5 * period1;
  timer1_isr_init();
}

void LwTx::lwtx_set_tick_counts(uint8_t low_count, uint8_t high_count, uint8_t trail_count, uint8_t gap_count) {
  this->tx_low_count = low_count;
  this->tx_high_count = high_count;
  this->tx_trail_count = trail_count;
  this->tx_gap_count = gap_count;
}

void LwTx::lwtx_set_gap_multiplier(uint8_t gap_multiplier) { this->tx_gap_multiplier = gap_multiplier; }

void LwTx::lw_timer_start() {
  {
    InterruptLock lock;
    static LwTx *arg = this;  // NOLINT
    timer1_attachInterrupt([] { isr_t_xtimer(arg); });
    timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);
    timer1_write(this->espPeriod);
  }
}

void LwTx::lw_timer_stop() {
  {
    InterruptLock lock;
    timer1_disable();
    timer1_detachInterrupt();
  }
}

}  // namespace lightwaverf
}  // namespace esphome
#endif
