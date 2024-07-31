#pragma once

#include "opentherm_component.h"
#include "unique_queue.h"

#include "esphome/core/component.h"

#include <OpenTherm.h>

#include <unordered_set>

namespace esphome {
    namespace opentherm {

        typedef void (*handle_interrupt_t)();

        typedef void (*process_response_t)(unsigned long, OpenThermResponseStatus);

        class OpenthermHub : public Component {
        private:
            class OpenthermStatusListener : public OpenthermComponent {
            public:
                OpenthermStatusListener();

                void process_response(const unsigned long response) override;

                unsigned int build_request(const unsigned int data) override;
            };

            /** TODO */
            OpenTherm ot;

            /** TODO */
            const handle_interrupt_t handle_interrupt_cb;

            /** TODO */
            const process_response_t process_response_cb;

            /** TODO */
            std::unordered_set<OpenthermComponent *> components;

            /** TODO */
            unique_queue<OpenThermMessageID> queue;

            /** TODO */
            OpenthermStatusListener status_listener;

            [[nodiscard]] unsigned int build_request(OpenThermMessageID id);

            [[nodiscard]] OpenThermMessageType get_message_type(OpenThermMessageID id);

        public:
            /**
             *
             * @param in_pin
             * @param out_pin
             * @param is_thermostat
             * @param handle_interrupt_cb
             * @param process_response_cb
             */
            OpenthermHub(int in_pin, int out_pin, bool is_thermostat, handle_interrupt_t handle_interrupt_cb,
                         process_response_t process_response_cb);

            void IRAM_ATTR

            handle_interrupt();

            void process_response(unsigned long response, OpenThermResponseStatus status);

            void register_component(OpenthermComponent *component);

            [[nodiscard]] float get_setup_priority() const override { return setup_priority::HARDWARE; }

            void setup() override;

            void on_shutdown() override;

            void loop() override;

            void dump_config() override;
        };

    } /* namespace opentherm */
} /* namespace esphome */
