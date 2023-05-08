

#include "esphome/core/log.h"

#ifdef USE_ESP8266

#include "lightwaverf.h"
#include "LwRx.h"
#include "LwTx.h"

namespace esphome {
namespace lightwaverf {

static const char *const TAG = "lightwaverf.sensor";

static const uint8_t DEFAULT_REPEAT = 10;
static const uint8_t DEFAULT_INVERT = 0;
static const uint32_t DEFAULT_TICK = 330;

void LightWaveRF::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Lightwave RF...");

  this->lwtx.lwtx_setup(pin_tx_, DEFAULT_REPEAT, DEFAULT_INVERT, DEFAULT_TICK);
  this->lwrx.lwrx_setup(pin_rx_);
}

void LightWaveRF::update() {
  ESP_LOGCONFIG(TAG, "update method ...");

  this->read_tx();
}

void LightWaveRF::read_tx() {
  if (this->lwrx.lwrx_message()) {
    this->lwrx.lwrx_getmessage(msg, msglen);
    printMsg(msg, msglen);
  }
}

void LightWaveRF::send_rx(uint8_t *msg, uint8_t repeats, uint8_t invert, int uSec) {
  this->lwtx.lwtx_setup(pin_tx_, repeats, invert, uSec);

  long timeout = 0;
  if (this->lwtx.lwtx_free()) {
    this->lwtx.lwtx_send(msg);
    timeout = millis();
    ESP_LOGD(TAG, "[%i] msg start", timeout);
  }
  while (!this->lwtx.lwtx_free() && millis() < (timeout + 1000)) {
    delay(10);
  }
  timeout = millis() - timeout;
  ESP_LOGD(TAG, "[%i] msg sent: %i", millis(), timeout);
}

void LightWaveRF::printMsg(uint8_t *msg, uint8_t len) {
  char buffer[65];
  ESP_LOGD(TAG, " Received code (len:%i): ", len);

  for (int i = 0; i < len; i++) {
    sprintf(&buffer[i * 6], "0x%02x, ", msg[i]);
    // ESP_LOGD(TAG, "%x ",   msg[i]);
  }
  ESP_LOGD(TAG, "[%s]", buffer);
}

void LightWaveRF::dump_config() {
  ESP_LOGCONFIG(TAG, "Lightwave RF:");
  LOG_PIN("  Pin TX: ", this->pin_tx_);
  LOG_PIN("  Pin RX: ", this->pin_rx_);
  LOG_UPDATE_INTERVAL(this);
}
}  // namespace lightwaverf
}  // namespace esphome

#endif