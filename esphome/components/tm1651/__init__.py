from esphome import automation, pins
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_BRIGHTNESS,
    CONF_CLK_PIN,
    CONF_DIO_PIN,
    CONF_ID,
    CONF_LEVEL,
)
from esphome.types import ConfigType

CODEOWNERS = ["@mrtoy-me"]

CONF_LEVEL_PERCENT = "level_percent"

tm1651_ns = cg.esphome_ns.namespace("tm1651")
TM1651Brightness = tm1651_ns.enum("TM1651Brightness")
TM1651Display = tm1651_ns.class_("TM1651Display", cg.Component)


TM1651_BRIGHTNESS_OPTIONS = {
    1: TM1651Brightness.TM1651_DARKEST,
    2: TM1651Brightness.TM1651_TYPICAL,
    3: TM1651Brightness.TM1651_BRIGHTEST,
}

MULTI_CONF = True

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TM1651Display),
            cv.Required(CONF_CLK_PIN): pins.internal_gpio_output_pin_schema,
            cv.Required(CONF_DIO_PIN): pins.internal_gpio_output_pin_schema,
        }
    ).extend(cv.COMPONENT_SCHEMA),
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    clk_pin = await cg.gpio_pin_expression(config[CONF_CLK_PIN])
    cg.add(var.set_clk_pin(clk_pin))
    dio_pin = await cg.gpio_pin_expression(config[CONF_DIO_PIN])
    cg.add(var.set_dio_pin(dio_pin))


validate_brightness = cv.enum(TM1651_BRIGHTNESS_OPTIONS, int=True)
validate_level = cv.All(cv.int_range(min=0, max=7))
validate_level_percent = cv.All(cv.int_range(min=0, max=100))

BINARY_OUTPUT_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(TM1651Display),
    }
)


for _name, _key, _validator, _method in (
    ("tm1651.set_brightness", CONF_BRIGHTNESS, validate_brightness, "set_brightness"),
    ("tm1651.set_level", CONF_LEVEL, validate_level, "set_level"),
    (
        "tm1651.set_level_percent",
        CONF_LEVEL_PERCENT,
        validate_level_percent,
        "set_level_percent",
    ),
):
    automation.register_apply_action(
        _name,
        cv.maybe_simple_value(
            {
                cv.GenerateID(): cv.use_id(TM1651Display),
                cv.Required(_key): cv.templatable(_validator),
            },
            key=_key,
        ),
        automation.ApplyField(_key, _method, cg.uint8),
    )

automation.register_apply_action(
    "tm1651.turn_off", BINARY_OUTPUT_ACTION_SCHEMA, automation.ApplyCall("turn_off()")
)
automation.register_apply_action(
    "tm1651.turn_on", BINARY_OUTPUT_ACTION_SCHEMA, automation.ApplyCall("turn_on()")
)
