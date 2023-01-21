#include "lightwaverf.h"
#include "LwRx.h"
#include "LwTx.h"

#include "esphome/core/log.h"

#ifdef USE_ESP8266

namespace esphome {
namespace lightwaverf {

    static const char *const TAG = "lightwaverf.sensor";

    static const uint8_t DEFAULT_REPEAT = 10;
    static const uint8_t DEFAULT_INVERT = 0;
    static const uint32_t DEFAULT_TICK = 330;
    
    void LightWaveRF::setup() {
        ESP_LOGCONFIG(TAG, "Setting up Lightwave RF...");

        lwtx_setup(pin_tx_, DEFAULT_REPEAT, DEFAULT_INVERT, DEFAULT_TICK);
        lwrx_setup(pin_rx_);
    }

    void LightWaveRF::update() {
        this->read_tx();
    }

    void LightWaveRF::read_tx() {
      if (lwrx_message()) {
         lwrx_getmessage(msg, msglen);
         printMsg(msg, msglen);
      }
    }

    void LightWaveRF::send_rx(byte *msg, byte repeats, byte invert, int uSec) {
        lwtx_setup(pin_tx, repeats, invert, uSec);

        long timeout = 0;
         if (lwtx_free()) {
            lwtx_send(msg);
            timeout = millis();
            ESP_LOGD(TAG, "[%i] msg start", timeout);
         }
         while(!lwtx_free() && millis() < (timeout + 1000)) {
            delay(10);
         }
         timeout = millis() - timeout;
         ESP_LOGD(TAG, "[%i] msg sent: %i",millis(), timeout);
    }

    void LightWaveRF::printMsg(byte *msg, byte len)  {
        char buffer[65];
        int j = 0;
        for(int i=0;i<len;i++) {
            buffer[j] =msg[i];
            j++;
            buffer[j] = ",";
            j++;
        }
        ESP_LOGD(TAG, " Received code: %s", buffer)
    }

    void LightWaveRF::dump_config() {
        ESP_LOGCONFIG(TAG, "Lightwave RF:");
        LOG_PIN("  Pin TX: ", this->pin_tx_);
        LOG_PIN("  Pin RX: ", this->pin_rx_);
        LOG_UPDATE_INTERVAL(this);
    }
}
}

#endif