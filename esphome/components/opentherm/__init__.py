from typing import Any

from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, PLATFORM_ESP32, PLATFORM_ESP8266

CODEOWNERS = ["@olegtarasov"]
MULTI_CONF = True

CONF_IN_PIN = "in_pin"
CONF_OUT_PIN = "out_pin"
CONF_CH_ENABLE = "ch_enable"
CONF_DHW_ENABLE = "dhw_enable"
CONF_COOLING_ENABLE = "cooling_enable"
CONF_OTC_ACTIVE = "otc_active"
CONF_CH2_ACTIVE = "ch2_active"
CONF_SYNC_MODE = "sync_mode"

opentherm_ns = cg.esphome_ns.namespace("opentherm")
OpenthermHub = opentherm_ns.class_("OpenthermHub", cg.Component)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(OpenthermHub),
            cv.Required(CONF_IN_PIN): pins.internal_gpio_input_pin_schema,
            cv.Required(CONF_OUT_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_CH_ENABLE, True): cv.boolean,
            cv.Optional(CONF_DHW_ENABLE, True): cv.boolean,
            cv.Optional(CONF_COOLING_ENABLE, False): cv.boolean,
            cv.Optional(CONF_OTC_ACTIVE, False): cv.boolean,
            cv.Optional(CONF_CH2_ACTIVE, False): cv.boolean,
            cv.Optional(CONF_SYNC_MODE, False): cv.boolean,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on([PLATFORM_ESP32, PLATFORM_ESP8266]),
)


async def to_code(config: dict[str, Any]) -> None:
    # Create the hub, passing the two callbacks defined below
    # Since the hub is used in the callbacks, we need to define it first
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Set pins
    in_pin = await cg.gpio_pin_expression(config[CONF_IN_PIN])
    cg.add(var.set_in_pin(in_pin))

    out_pin = await cg.gpio_pin_expression(config[CONF_OUT_PIN])
    cg.add(var.set_out_pin(out_pin))

    non_sensors = {CONF_ID, CONF_IN_PIN, CONF_OUT_PIN}
    for key, value in config.items():
        if key not in non_sensors:
            cg.add(getattr(var, f"set_{key}")(value))
