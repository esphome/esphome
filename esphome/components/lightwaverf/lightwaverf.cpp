#include "esphome/core/log.h"

#ifdef USE_ESP8266

#include "lightwaverf.h"

namespace esphome {
namespace lightwaverf {

static const char *const TAG = "lightwaverf.sensor";

static const uint8_t DEFAULT_REPEAT = 10;
static const bool DEFAULT_INVERT = false;
static const uint32_t DEFAULT_TICK = 330;

void LightWaveRF::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Lightwave RF...");

  this->lwtx_.lwtx_setup(pin_tx_, DEFAULT_REPEAT, DEFAULT_INVERT, DEFAULT_TICK);
  this->lwrx_.lwrx_setup(pin_rx_);
}

void LightWaveRF::update() { this->read_tx(); }

void LightWaveRF::read_tx() {
  if (this->lwrx_.lwrx_message()) {
    this->lwrx_.lwrx_getmessage(msg_, msglen_);
    print_msg_(msg_, msglen_);
  }
}

void LightWaveRF::send_rx(const std::vector<uint8_t> &msg, uint8_t repeats, bool inverted, int u_sec) {
  this->lwtx_.lwtx_setup(pin_tx_, repeats, inverted, u_sec);

  uint32_t timeout = 0;
  if (this->lwtx_.lwtx_free()) {
    this->lwtx_.lwtx_send(msg);
    timeout = millis();
    ESP_LOGD(TAG, "[%i] msg start", timeout);
  }
  while (!this->lwtx_.lwtx_free() && millis() < (timeout + 1000)) {
    delay(10);
  }
  timeout = millis() - timeout;
  ESP_LOGD(TAG, "[%u] msg sent: %i", millis(), timeout);
}

void LightWaveRF::print_msg_(uint8_t *msg, uint8_t len) {
  char buffer[65];
  ESP_LOGD(TAG, " Received code (len:%i): ", len);

  for (int i = 0; i < len; i++) {
    sprintf(&buffer[i * 6], "0x%02x, ", msg[i]);
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
