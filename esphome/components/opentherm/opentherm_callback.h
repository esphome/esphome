#pragma once

#include "functions.h"

namespace esphome {
    namespace opentherm {
        template <typename T, typename Parse, typename Write>
        class OpenthermCallback {
        public:
            /** TODO */
            OpenthermCallback(Parse parse, Write write) : parse(parse), write(write) {
                /* Do nothing */
            }

        protected:
            /** TODO */
            const Parse parse;

            /** TODO */
            const Write write;
        };
    }
}