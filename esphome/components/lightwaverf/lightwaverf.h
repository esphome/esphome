
#pragma once

#include "esphome/core/component.h"

namespace esphome {
namespace lightwaverf {
    #ifdef USE_ESP8266


    class LightWaveRF : public PollingComponent {

        LightWaveRF() {}

        public:
            void set_pin(int *pin_tx, int *pin_rx) { pin_tx_ = pin_tx; pin_rx_ = pin_rx; }
            void update() override;
            void setup() override;
            void dump_config() override;
            void read_tx();
            void send_rx(byte *msg, byte repeats, byte invert, int uSec);

        protected:
            void printMsg(byte *msg, byte len);
            byte msg[10];
            byte msglen = 10;
            int *pin_tx_;
            int *pin_rx_;

    }

    #endif
} // namespace lightwaverf
} // namespace esphome