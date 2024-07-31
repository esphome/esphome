#pragma once

#include "esphome/core/component.h"

#include <OpenTherm.h>

namespace esphome {
    namespace opentherm {

        class OpenthermComponent {
        public:

            OpenthermComponent(OpenThermMessageID message_id,
                               bool keep_updated)
                    : message_id(message_id), keep_updated(keep_updated) {

            }

            virtual void process_response(const unsigned long response) = 0;

            virtual unsigned int build_request(const unsigned int data) = 0;

            const OpenThermMessageID message_id;

            const bool keep_updated;

        };

    } /* namespace opentherm */
} /* namespace esphome */
