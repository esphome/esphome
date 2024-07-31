#pragma once

#include "../opentherm_component.h"
#include "../opentherm_callback.h"

#include "esphome/components/switch/switch.h"
#include "esphome/core/log.h"

namespace esphome {
    namespace opentherm {

        template<typename T, typename Parse = std::function<T(const unsigned long)>, typename Write = std::function<unsigned int(const T, const unsigned int)>>
        class OpenthermSwitch
                : public OpenthermComponent,
                  public OpenthermCallback<T, Parse, Write>,
                  public switch_::Switch,
                  public Component {
        private:
            constexpr static const char *const TAG = "Opentherm.switch";

        public:
            OpenthermSwitch(OpenThermMessageID message_id, bool keep_updated, Parse parse, Write write)
                    : OpenthermComponent(message_id, keep_updated),
                      OpenthermCallback<T, Parse, Write>(parse, write),
                      switch_::Switch(),
                      Component() {

            }

            void process_response(const unsigned long response) override {
                write_state(static_cast<bool>(this->parse(response)));
            }

            unsigned int build_request(const unsigned int data) override {
                return this->write(static_cast<T>(state), data);
            }

            void write_state(bool state) override {
                publish_state(state);
            }

            void dump_config() override {
                LOG_SWITCH(TAG, "OpenthermSwitch", this);
            }
        };

    }  // namespace opentherm
}  // namespace esphome
