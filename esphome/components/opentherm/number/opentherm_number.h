#pragma once

#include "../opentherm_component.h"
#include "../opentherm_callback.h"
#include "../opentherm_input.h"

#include "esphome/components/number/number.h"
#include "esphome/core/log.h"

namespace esphome {
    namespace opentherm {

        template<typename T, typename Parse = std::function<T(const unsigned long)>, typename Write = std::function<unsigned int(const T, const unsigned int)>>
        class OpenthermNumber
                : public OpenthermComponent,
                  public OpenthermCallback<T, Parse, Write>,
                  public OpenthermInput,
                  public number::Number,
                  public Component {
        private:
            constexpr static const char *TAG = "Opentherm.number";

        public:
            OpenthermNumber(OpenThermMessageID message_id, bool keep_updated, Parse parse, Write write)
                    : OpenthermComponent(message_id, keep_updated),
                      OpenthermCallback<T, Parse, Write>(parse, write),
                      OpenthermInput(),
                      number::Number(),
                      Component() {

            }

            void process_response(const unsigned long response) override {
                control(static_cast<float>(this->parse(response)));
            }

            unsigned int build_request(const unsigned int data) override {
                return this->write(static_cast<T>(state), data);
            }

            void dump_config() override {
                LOG_NUMBER(TAG, "OpenthermNumber", this);
            }

        protected:
            void control(float value) override {
                publish_state(value);
            }
        };

    }  // namespace opentherm
}  // namespace esphome
