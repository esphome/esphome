#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/i2c/i2c_bus.h"

namespace esphome
{
    namespace kmeteriso
    {

        /// This class implements support for the KMeterISO thermocouple sensor.
        class KMeterISOComponent : public PollingComponent, public i2c::I2CDevice
        {
        public:
            void set_temperature_sensor(sensor::Sensor *t) { temperature_sensor_ = t; }
            void set_internal_temperature_sensor(sensor::Sensor *t) { internal_temperature_sensor_ = t; }

            // ========== INTERNAL METHODS ==========
            // (In most use cases you won't need these)
            void setup() override;
            float get_setup_priority() const override;
            void update() override;

        protected:
            /// Read the temperature value and store the calculated ambient temperature in t_fine.
            float read_temperature_(const uint8_t *data, int32_t *t_fine);

            sensor::Sensor *temperature_sensor_{nullptr};
            sensor::Sensor *internal_temperature_sensor_{nullptr};
            enum ErrorCode
            {
                NONE = 0,
                COMMUNICATION_FAILED,
                STATUS_FAILED,
            } error_code_{NONE};
        };

    } // namespace kmeteriso
} // namespace esphome
