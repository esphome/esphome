import esphome.codegen as cg
from esphome import pins
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_INPUT,
)
from esphome.core import CORE

CODEOWNERS = ["@asoehlke", "rmorenoramos"]
AUTO_LOAD = ["sensor", "voltage_sampler"]

cd74hc4067_ns = cg.esphome_ns.namespace("cd74hc4067")

CD74HC4067Component = cd74hc4067_ns.class_(
    "CD74HC4067Component", cg.Component, cg.PollingComponent
)

CONF_PIN_ADC = "pin_adc"
CONF_PIN_S0 = "pin_s0"
CONF_PIN_S1 = "pin_s1"
CONF_PIN_S2 = "pin_s2"
CONF_PIN_S3 = "pin_s3"


def validate_adc_pin(value):
    if str(value).upper() == "VCC":
        return cv.only_on_esp8266("VCC")

    if CORE.is_esp32:
        from esphome.components.esp32 import is_esp32c3

        value = pins.internal_gpio_input_pin_number(value)
        if is_esp32c3():
            if not (0 <= value <= 4):  # ADC1
                raise cv.Invalid("ESP32-C3: Only pins 0 though 4 support ADC.")
        if not (32 <= value <= 39):  # ADC1
            raise cv.Invalid("ESP32: Only pins 32 though 39 support ADC.")
    elif CORE.is_esp8266:
        from esphome.components.esp8266.gpio import CONF_ANALOG

        value = pins.internal_gpio_pin_number({CONF_ANALOG: True, CONF_INPUT: True})(
            value
        )

        if value != 17:  # A0
            raise cv.Invalid("ESP8266: Only pin A0 (GPIO17) supports ADC.")
        return pins.gpio_pin_schema(
            {CONF_ANALOG: True, CONF_INPUT: True}, internal=True
        )(value)
    else:
        raise NotImplementedError

    return pins.internal_gpio_input_pin_schema(value)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CD74HC4067Component),
        cv.Required(CONF_PIN_ADC): validate_adc_pin,
        cv.Required(CONF_PIN_S0): pins.internal_gpio_output_pin_schema,
        cv.Required(CONF_PIN_S1): pins.internal_gpio_output_pin_schema,
        cv.Required(CONF_PIN_S2): pins.internal_gpio_output_pin_schema,
        cv.Required(CONF_PIN_S3): pins.internal_gpio_output_pin_schema,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    pin_adc = await cg.gpio_pin_expression(config[CONF_PIN_ADC])
    cg.add(var.set_pin_adc(pin_adc))
    pin_s0 = await cg.gpio_pin_expression(config[CONF_PIN_S0])
    cg.add(var.set_pin_s0(pin_s0))
    pin_s1 = await cg.gpio_pin_expression(config[CONF_PIN_S1])
    cg.add(var.set_pin_s1(pin_s1))
    pin_s2 = await cg.gpio_pin_expression(config[CONF_PIN_S2])
    cg.add(var.set_pin_s2(pin_s2))
    pin_s3 = await cg.gpio_pin_expression(config[CONF_PIN_S3])
    cg.add(var.set_pin_s3(pin_s3))
