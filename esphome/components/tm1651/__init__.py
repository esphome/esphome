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

CODEOWNERS = ["@freekode"]

tm1651_ns = cg.esphome_ns.namespace("tm1651")
TM1651Brightness = tm1651_ns.enum("TM1651Brightness")
TM1651Display = tm1651_ns.class_("TM1651Display", cg.Component)

SetLevelPercentAction = tm1651_ns.class_("SetLevelPercentAction", automation.Action)
SetLevelAction = tm1651_ns.class_("SetLevelAction", automation.Action)
SetBrightnessAction = tm1651_ns.class_("SetBrightnessAction", automation.Action)
TurnOnAction = tm1651_ns.class_("SetLevelPercentAction", automation.Action)
TurnOffAction = tm1651_ns.class_("SetLevelPercentAction", automation.Action)

CONF_LEVEL_PERCENT = "level_percent"

TM1651_BRIGHTNESS_OPTIONS = {
    1: TM1651Brightness.TM1651_BRIGHTNESS_LOW,
    2: TM1651Brightness.TM1651_BRIGHTNESS_MEDIUM,
    3: TM1651Brightness.TM1651_BRIGHTNESS_HIGH,
}

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TM1651Display),
            cv.Required(CONF_CLK_PIN): pins.internal_gpio_output_pin_schema,
            cv.Required(CONF_DIO_PIN): pins.internal_gpio_output_pin_schema,
        }
    ),
    cv.only_with_arduino,
)

validate_level_percent = cv.All(cv.int_range(min=0, max=100))
validate_level = cv.All(cv.int_range(min=0, max=7))
validate_brightness = cv.enum(TM1651_BRIGHTNESS_OPTIONS, int=True)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    clk_pin = await cg.gpio_pin_expression(config[CONF_CLK_PIN])
    cg.add(var.set_clk_pin(clk_pin))
    dio_pin = await cg.gpio_pin_expression(config[CONF_DIO_PIN])
    cg.add(var.set_dio_pin(dio_pin))

    # https://platformio.org/lib/show/6865/TM1651
    cg.add_library("freekode/TM1651", "1.0.1")


BINARY_OUTPUT_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(TM1651Display),
    }
)


@automation.register_action("tm1651.turn_on", TurnOnAction, BINARY_OUTPUT_ACTION_SCHEMA)
async def output_turn_on_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "tm1651.turn_off", TurnOffAction, BINARY_OUTPUT_ACTION_SCHEMA
)
async def output_turn_off_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "tm1651.set_level_percent",
    SetLevelPercentAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(TM1651Display),
            cv.Required(CONF_LEVEL_PERCENT): cv.templatable(validate_level_percent),
        },
        key=CONF_LEVEL_PERCENT,
    ),
)
async def tm1651_set_level_percent_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    template_ = await cg.templatable(config[CONF_LEVEL_PERCENT], args, cg.uint8)
    cg.add(var.set_level_percent(template_))
    return var


@automation.register_action(
    "tm1651.set_level",
    SetLevelAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(TM1651Display),
            cv.Required(CONF_LEVEL): cv.templatable(validate_level),
        },
        key=CONF_LEVEL,
    ),
)
async def tm1651_set_level_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    template_ = await cg.templatable(config[CONF_LEVEL], args, cg.uint8)
    cg.add(var.set_level(template_))
    return var


@automation.register_action(
    "tm1651.set_brightness",
    SetBrightnessAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(TM1651Display),
            cv.Required(CONF_BRIGHTNESS): cv.templatable(validate_brightness),
        },
        key=CONF_BRIGHTNESS,
    ),
)
async def tm1651_set_brightness_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    template_ = await cg.templatable(config[CONF_BRIGHTNESS], args, cg.uint8)
    cg.add(var.set_brightness(template_))
    return var
