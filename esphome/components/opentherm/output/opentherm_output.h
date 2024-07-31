#pragma once

#include "../opentherm_component.h"
#include "../opentherm_callback.h"
#include "../opentherm_input.h"

#include "esphome/components/output/float_output.h"
#include "esphome/core/log.h"

namespace esphome {
    namespace opentherm {

        template<typename T, typename Parse = std::function<T(const unsigned long)>, typename Write = std::function<unsigned int(const T, const unsigned int)>>
        class OpenthermOutput
                : public OpenthermComponent,
                  public OpenthermCallback<T, Parse, Write>,
                  public OpenthermInput,
                  public output::FloatOutput,
                  public Component {
        private:
            constexpr static const char *TAG = "Opentherm.output";

            bool has_state_ = false;

        public:
            OpenthermOutput(OpenThermMessageID message_id, bool keep_updated, Parse parse, Write write)
                    : OpenthermComponent(message_id, keep_updated),
                      OpenthermCallback<T, Parse, Write>(parse, write),
                      OpenthermInput(),
                      output::FloatOutput(),
                      Component() {

            }

            void process_response(const unsigned long response) override {
                write_state(static_cast<float>(this->parse(response)));
            }

            unsigned int build_request(const unsigned int data) override {
                return this->write(static_cast<T>(state), data);
            }

            bool has_state() {
                return has_state_;
            }

            void dump_config() override {
                LOG_FLOAT_OUTPUT(this);
            }

            float state;

        protected:
            void write_state(float value) override {
                state = value < 0.003 && zero_means_zero_ ? 0.0 : min_value + value * (max_value - min_value);
                has_state_ = true;
                ESP_LOGD(TAG, "Output set to %.2f", state);
            }
        };

    }  // namespace opentherm
}  // namespace esphome
