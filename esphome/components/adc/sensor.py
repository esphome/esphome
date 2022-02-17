import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import sensor, voltage_sampler
from esphome.const import (
    CONF_ATTENUATION,
    CONF_RAW,
    CONF_ID,
    CONF_INPUT,
    CONF_NUMBER,
    CONF_PIN,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_VOLT,
)
from esphome.core import CORE
from esphome.components.esp32 import get_esp32_variant
from esphome.components.esp32.const import (
    VARIANT_ESP32,
    VARIANT_ESP32C3,
    VARIANT_ESP32H2,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
)


AUTO_LOAD = ["voltage_sampler"]

ATTENUATION_MODES = {
    "0db": cg.global_ns.ADC_ATTEN_DB_0,
    "2.5db": cg.global_ns.ADC_ATTEN_DB_2_5,
    "6db": cg.global_ns.ADC_ATTEN_DB_6,
    "11db": cg.global_ns.ADC_ATTEN_DB_11,
    "auto": "auto",
}

adc1_channel_t = cg.global_ns.enum("adc1_channel_t")

# From https://github.com/espressif/esp-idf/blob/master/components/driver/include/driver/adc_common.h
# pin to adc1 channel mapping
ESP32_VARIANT_ADC1_PIN_TO_CHANNEL = {
    VARIANT_ESP32: {
        36: adc1_channel_t.ADC1_CHANNEL_0,
        37: adc1_channel_t.ADC1_CHANNEL_1,
        38: adc1_channel_t.ADC1_CHANNEL_2,
        39: adc1_channel_t.ADC1_CHANNEL_3,
        32: adc1_channel_t.ADC1_CHANNEL_4,
        33: adc1_channel_t.ADC1_CHANNEL_5,
        34: adc1_channel_t.ADC1_CHANNEL_6,
        35: adc1_channel_t.ADC1_CHANNEL_7,
    },
    VARIANT_ESP32S2: {
        1: adc1_channel_t.ADC1_CHANNEL_0,
        2: adc1_channel_t.ADC1_CHANNEL_1,
        3: adc1_channel_t.ADC1_CHANNEL_2,
        4: adc1_channel_t.ADC1_CHANNEL_3,
        5: adc1_channel_t.ADC1_CHANNEL_4,
        6: adc1_channel_t.ADC1_CHANNEL_5,
        7: adc1_channel_t.ADC1_CHANNEL_6,
        8: adc1_channel_t.ADC1_CHANNEL_7,
        9: adc1_channel_t.ADC1_CHANNEL_8,
        10: adc1_channel_t.ADC1_CHANNEL_9,
    },
    VARIANT_ESP32S3: {
        1: adc1_channel_t.ADC1_CHANNEL_0,
        2: adc1_channel_t.ADC1_CHANNEL_1,
        3: adc1_channel_t.ADC1_CHANNEL_2,
        4: adc1_channel_t.ADC1_CHANNEL_3,
        5: adc1_channel_t.ADC1_CHANNEL_4,
        6: adc1_channel_t.ADC1_CHANNEL_5,
        7: adc1_channel_t.ADC1_CHANNEL_6,
        8: adc1_channel_t.ADC1_CHANNEL_7,
        9: adc1_channel_t.ADC1_CHANNEL_8,
        10: adc1_channel_t.ADC1_CHANNEL_9,
    },
    VARIANT_ESP32C3: {
        0: adc1_channel_t.ADC1_CHANNEL_0,
        1: adc1_channel_t.ADC1_CHANNEL_1,
        2: adc1_channel_t.ADC1_CHANNEL_2,
        3: adc1_channel_t.ADC1_CHANNEL_3,
        4: adc1_channel_t.ADC1_CHANNEL_4,
    },
    VARIANT_ESP32H2: {
        0: adc1_channel_t.ADC1_CHANNEL_0,
        1: adc1_channel_t.ADC1_CHANNEL_1,
        2: adc1_channel_t.ADC1_CHANNEL_2,
        3: adc1_channel_t.ADC1_CHANNEL_3,
        4: adc1_channel_t.ADC1_CHANNEL_4,
    },
}


def validate_adc_pin(value):
    if str(value).upper() == "VCC":
        return cv.only_on_esp8266("VCC")

    if CORE.is_esp32:
        value = pins.internal_gpio_input_pin_number(value)
        variant = get_esp32_variant()
        if variant not in ESP32_VARIANT_ADC1_PIN_TO_CHANNEL:
            raise cv.Invalid(f"This ESP32 variant ({variant}) is not supported")

        if value not in ESP32_VARIANT_ADC1_PIN_TO_CHANNEL[variant]:
            raise cv.Invalid(f"{variant} doesn't support ADC on this pin")
        return pins.internal_gpio_input_pin_schema(value)

    if CORE.is_esp8266:
        from esphome.components.esp8266.gpio import CONF_ANALOG

        value = pins.internal_gpio_pin_number({CONF_ANALOG: True, CONF_INPUT: True})(
            value
        )

        if value != 17:  # A0
            raise cv.Invalid("ESP8266: Only pin A0 (GPIO17) supports ADC.")
        return pins.gpio_pin_schema(
            {CONF_ANALOG: True, CONF_INPUT: True}, internal=True
        )(value)

    raise NotImplementedError


def validate_config(config):
    if config[CONF_RAW] and config.get(CONF_ATTENUATION, None) == "auto":
        raise cv.Invalid("Automatic attenuation cannot be used when raw output is set.")
    return config


adc_ns = cg.esphome_ns.namespace("adc")
ADCSensor = adc_ns.class_(
    "ADCSensor", sensor.Sensor, cg.PollingComponent, voltage_sampler.VoltageSampler
)

CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        ADCSensor,
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.Required(CONF_PIN): validate_adc_pin,
            cv.Optional(CONF_RAW, default=False): cv.boolean,
            cv.SplitDefault(CONF_ATTENUATION, esp32="0db"): cv.All(
                cv.only_on_esp32, cv.enum(ATTENUATION_MODES, lower=True)
            ),
        }
    )
    .extend(cv.polling_component_schema("60s")),
    validate_config,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    if config[CONF_PIN] == "VCC":
        cg.add_define("USE_ADC_SENSOR_VCC")
    else:
        pin = await cg.gpio_pin_expression(config[CONF_PIN])
        cg.add(var.set_pin(pin))

    if CONF_RAW in config:
        cg.add(var.set_output_raw(config[CONF_RAW]))

    if CONF_ATTENUATION in config:
        if config[CONF_ATTENUATION] == "auto":
            cg.add(var.set_autorange(cg.global_ns.true))
        else:
            cg.add(var.set_attenuation(config[CONF_ATTENUATION]))

    if CORE.is_esp32:
        variant = get_esp32_variant()
        pin_num = config[CONF_PIN][CONF_NUMBER]
        chan = ESP32_VARIANT_ADC1_PIN_TO_CHANNEL[variant][pin_num]
        cg.add(var.set_channel(chan))
