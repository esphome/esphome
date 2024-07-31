#pragma once

#include "../opentherm_component.h"
#include "../opentherm_callback.h"

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
    namespace opentherm {

        template<typename T, typename Parse = std::function<T(const unsigned long)>, typename Write = std::function<unsigned int(const T, const unsigned int)>>
        class OpenthermBinarySensor
                : public OpenthermComponent,
                  public OpenthermCallback<T, Parse, Write>,
                  public binary_sensor::BinarySensor,
                  public Component {
        private:
            constexpr static const char *TAG = "Opentherm.binary_sensor";

        public:
            OpenthermBinarySensor(OpenThermMessageID message_id, bool keep_updated, Parse parse, Write write)
                    : OpenthermComponent(message_id, keep_updated),
                      OpenthermCallback<T, Parse, Write>(parse, write),
                      binary_sensor::BinarySensor(),
                      Component() {

            }

            void process_response(const unsigned long response) override {
                publish_state(static_cast<bool>(this->parse(response)));
            }

            unsigned int build_request(const unsigned int data) override {
                return this->write(static_cast<T>(state), data);
            }

            void dump_config() override {
                LOG_BINARY_SENSOR(TAG, "OpenthermBinarySensor", this);
            }
        };

    }  // namespace opentherm
}  // namespace esphome
