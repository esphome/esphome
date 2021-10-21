import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import sensor, voltage_sampler
from esphome.const import (
    CONF_ATTENUATION,
    CONF_ID,
    CONF_INPUT,
    CONF_PIN,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_VOLT,
)
from esphome.core import CORE


AUTO_LOAD = ["voltage_sampler"]

ATTENUATION_MODES = {
    "0db": cg.global_ns.ADC_ATTEN_DB_0,
    "2.5db": cg.global_ns.ADC_ATTEN_DB_2_5,
    "6db": cg.global_ns.ADC_ATTEN_DB_6,
    "11db": cg.global_ns.ADC_ATTEN_DB_11,
}


def validate_adc_pin(value):
    if str(value).upper() == "VCC":
        return cv.only_on_esp8266("VCC")

    if CORE.is_esp32:
        from esphome.components.esp32 import is_esp32c3

        value = pins.internal_gpio_input_pin_number(value)
        if is_esp32c3():
            if not (0 <= value <= 4):  # ADC1
                raise cv.Invalid("ESP32-C3: Only pins 0 though 4 support ADC.")
        elif not (32 <= value <= 39):  # ADC1
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


adc_ns = cg.esphome_ns.namespace("adc")
ADCSensor = adc_ns.class_(
    "ADCSensor", sensor.Sensor, cg.PollingComponent, voltage_sampler.VoltageSampler
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.GenerateID(): cv.declare_id(ADCSensor),
            cv.Required(CONF_PIN): validate_adc_pin,
            cv.SplitDefault(CONF_ATTENUATION, esp32="0db"): cv.All(
                cv.only_on_esp32, cv.enum(ATTENUATION_MODES, lower=True)
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
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

    if CONF_ATTENUATION in config:
        cg.add(var.set_attenuation(config[CONF_ATTENUATION]))
