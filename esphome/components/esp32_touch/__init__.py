import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_HIGH_VOLTAGE_REFERENCE,
    CONF_ID,
    CONF_IIR_FILTER,
    CONF_LOW_VOLTAGE_REFERENCE,
    CONF_MEASUREMENT_DURATION,
    CONF_SETUP_MODE,
    CONF_SLEEP_DURATION,
    CONF_VOLTAGE_ATTENUATION,
)
from esphome.core import TimePeriod

AUTO_LOAD = ["binary_sensor"]
DEPENDENCIES = ["esp32"]

esp32_touch_ns = cg.esphome_ns.namespace("esp32_touch")
ESP32TouchComponent = esp32_touch_ns.class_("ESP32TouchComponent", cg.Component)


def validate_voltage(values):
    def validator(value):
        if isinstance(value, float) and value.is_integer():
            value = int(value)
        value = cv.string(value)
        if not value.endswith("V"):
            value += "V"
        return cv.one_of(*values)(value)

    return validator


LOW_VOLTAGE_REFERENCE = {
    "0.5V": cg.global_ns.TOUCH_LVOLT_0V5,
    "0.6V": cg.global_ns.TOUCH_LVOLT_0V6,
    "0.7V": cg.global_ns.TOUCH_LVOLT_0V7,
    "0.8V": cg.global_ns.TOUCH_LVOLT_0V8,
}
HIGH_VOLTAGE_REFERENCE = {
    "2.4V": cg.global_ns.TOUCH_HVOLT_2V4,
    "2.5V": cg.global_ns.TOUCH_HVOLT_2V5,
    "2.6V": cg.global_ns.TOUCH_HVOLT_2V6,
    "2.7V": cg.global_ns.TOUCH_HVOLT_2V7,
}
VOLTAGE_ATTENUATION = {
    "1.5V": cg.global_ns.TOUCH_HVOLT_ATTEN_1V5,
    "1V": cg.global_ns.TOUCH_HVOLT_ATTEN_1V,
    "0.5V": cg.global_ns.TOUCH_HVOLT_ATTEN_0V5,
    "0V": cg.global_ns.TOUCH_HVOLT_ATTEN_0V,
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ESP32TouchComponent),
        cv.Optional(CONF_SETUP_MODE, default=False): cv.boolean,
        cv.Optional(
            CONF_IIR_FILTER, default="0ms"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_SLEEP_DURATION, default="27306us"): cv.All(
            cv.positive_time_period, cv.Range(max=TimePeriod(microseconds=436906))
        ),
        cv.Optional(CONF_MEASUREMENT_DURATION, default="8192us"): cv.All(
            cv.positive_time_period, cv.Range(max=TimePeriod(microseconds=8192))
        ),
        cv.Optional(CONF_LOW_VOLTAGE_REFERENCE, default="0.5V"): validate_voltage(
            LOW_VOLTAGE_REFERENCE
        ),
        cv.Optional(CONF_HIGH_VOLTAGE_REFERENCE, default="2.7V"): validate_voltage(
            HIGH_VOLTAGE_REFERENCE
        ),
        cv.Optional(CONF_VOLTAGE_ATTENUATION, default="0V"): validate_voltage(
            VOLTAGE_ATTENUATION
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    touch = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(touch, config)

    cg.add(touch.set_setup_mode(config[CONF_SETUP_MODE]))
    cg.add(touch.set_iir_filter(config[CONF_IIR_FILTER]))

    sleep_duration = int(round(config[CONF_SLEEP_DURATION].total_microseconds * 0.15))
    cg.add(touch.set_sleep_duration(sleep_duration))

    measurement_duration = int(
        round(config[CONF_MEASUREMENT_DURATION].total_microseconds * 7.99987793)
    )
    cg.add(touch.set_measurement_duration(measurement_duration))

    cg.add(
        touch.set_low_voltage_reference(
            LOW_VOLTAGE_REFERENCE[config[CONF_LOW_VOLTAGE_REFERENCE]]
        )
    )
    cg.add(
        touch.set_high_voltage_reference(
            HIGH_VOLTAGE_REFERENCE[config[CONF_HIGH_VOLTAGE_REFERENCE]]
        )
    )
    cg.add(
        touch.set_voltage_attenuation(
            VOLTAGE_ATTENUATION[config[CONF_VOLTAGE_ATTENUATION]]
        )
    )
