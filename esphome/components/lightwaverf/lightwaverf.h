
#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"

#include "LwRx.h"


namespace esphome {
namespace lightwaverf {
    #ifdef USE_ESP8266


    class LightWaveRF : public PollingComponent {

        public:
            void set_pin(InternalGPIOPin *pin_tx, InternalGPIOPin *pin_rx) { pin_tx_ = pin_tx; pin_rx_ = pin_rx; }
            void update() override;
            void setup() override;
            void dump_config() override;
            void read_tx();
            void send_rx(uint8_t *msg, uint8_t repeats, uint8_t invert, int uSec);

        protected:
            void printMsg(uint8_t *msg, uint8_t len);
            uint8_t msg[10];
            uint8_t msglen = 10;
            InternalGPIOPin *pin_tx_;
            InternalGPIOPin *pin_rx_;
            LwRx lwrx;
            //LwTx lwtx;

    };

    #endif
} 
} 