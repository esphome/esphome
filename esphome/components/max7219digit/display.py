import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import display, spi
from esphome import automation
from esphome.const import (
    CONF_ID,
    CONF_INTENSITY,
    CONF_LAMBDA,
    CONF_NUM_CHIPS,
    CONF_STATE,
)

CODEOWNERS = ["@rspaargaren"]
DEPENDENCIES = ["spi"]

CONF_ROTATE_CHIP = "rotate_chip"
CONF_FLIP_X = "flip_x"
CONF_SCROLL_SPEED = "scroll_speed"
CONF_SCROLL_DWELL = "scroll_dwell"
CONF_SCROLL_DELAY = "scroll_delay"
CONF_SCROLL_ENABLE = "scroll_enable"
CONF_SCROLL_MODE = "scroll_mode"
CONF_REVERSE_ENABLE = "reverse_enable"
CONF_NUM_CHIP_LINES = "num_chip_lines"
CONF_CHIP_LINES_STYLE = "chip_lines_style"


integration_ns = cg.esphome_ns.namespace("max7219digit")
ChipLinesStyle = integration_ns.enum("ChipLinesStyle")
CHIP_LINES_STYLE = {
    "ZIGZAG": ChipLinesStyle.ZIGZAG,
    "SNAKE": ChipLinesStyle.SNAKE,
}

ScrollMode = integration_ns.enum("ScrollMode")
SCROLL_MODES = {
    "CONTINUOUS": ScrollMode.CONTINUOUS,
    "STOP": ScrollMode.STOP,
}

CHIP_MODES = {
    "0": 0,
    "90": 1,
    "180": 2,
    "270": 3,
}

max7219_ns = cg.esphome_ns.namespace("max7219digit")
MAX7219Component = max7219_ns.class_(
    "MAX7219Component", spi.SPIDevice, display.DisplayBuffer, cg.PollingComponent
)
MAX7219ComponentRef = MAX7219Component.operator("ref")

CONFIG_SCHEMA = (
    display.BASIC_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(MAX7219Component),
            cv.Optional(CONF_NUM_CHIPS, default=4): cv.int_range(min=1, max=255),
            cv.Optional(CONF_NUM_CHIP_LINES, default=1): cv.int_range(min=1, max=255),
            cv.Optional(CONF_CHIP_LINES_STYLE, default="SNAKE"): cv.enum(
                CHIP_LINES_STYLE, upper=True
            ),
            cv.Optional(CONF_INTENSITY, default=15): cv.int_range(min=0, max=15),
            cv.Optional(CONF_ROTATE_CHIP, default="0"): cv.enum(CHIP_MODES, upper=True),
            cv.Optional(CONF_SCROLL_MODE, default="CONTINUOUS"): cv.enum(
                SCROLL_MODES, upper=True
            ),
            cv.Optional(CONF_SCROLL_ENABLE, default=True): cv.boolean,
            cv.Optional(
                CONF_SCROLL_SPEED, default="250ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_SCROLL_DELAY, default="1000ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_SCROLL_DWELL, default="1000ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_REVERSE_ENABLE, default=False): cv.boolean,
            cv.Optional(CONF_FLIP_X, default=False): cv.boolean,
        }
    )
    .extend(cv.polling_component_schema("500ms"))
    .extend(spi.spi_device_schema(cs_pin_required=True))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await spi.register_spi_device(var, config)
    await display.register_display(var, config)

    cg.add(var.set_num_chips(config[CONF_NUM_CHIPS]))
    cg.add(var.set_num_chip_lines(config[CONF_NUM_CHIP_LINES]))
    cg.add(var.set_chip_lines_style(config[CONF_CHIP_LINES_STYLE]))
    cg.add(var.set_intensity(config[CONF_INTENSITY]))
    cg.add(var.set_chip_orientation(config[CONF_ROTATE_CHIP]))
    cg.add(var.set_scroll_speed(config[CONF_SCROLL_SPEED]))
    cg.add(var.set_scroll_dwell(config[CONF_SCROLL_DWELL]))
    cg.add(var.set_scroll_delay(config[CONF_SCROLL_DELAY]))
    cg.add(var.set_scroll(config[CONF_SCROLL_ENABLE]))
    cg.add(var.set_scroll_mode(config[CONF_SCROLL_MODE]))
    cg.add(var.set_reverse(config[CONF_REVERSE_ENABLE]))
    cg.add(var.set_flip_x(config[CONF_FLIP_X]))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(MAX7219ComponentRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))


DisplayInvertAction = max7219_ns.class_("DisplayInvertAction", automation.Action)
DisplayVisiblityAction = max7219_ns.class_("DisplayVisiblityAction", automation.Action)
DisplayReverseAction = max7219_ns.class_("DisplayReverseAction", automation.Action)
DisplayIntensityAction = max7219_ns.class_("DisplayIntensityAction", automation.Action)


MAX7219_OFF_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(MAX7219Component),
        cv.Optional(CONF_STATE, default=False): False,
    }
)
MAX7219_ON_ACTION_SCHEMA = automation.maybe_simple_id(
    MAX7219_OFF_ACTION_SCHEMA.extend(
        {
            cv.Optional(CONF_STATE, default=True): True,
        }
    )
)


@automation.register_action(
    "MAX7219.invert_off", DisplayInvertAction, MAX7219_OFF_ACTION_SCHEMA
)
@automation.register_action(
    "MAX7219.inveert_on", DisplayInvertAction, MAX7219_ON_ACTION_SCHEMA
)
async def MAX7219_inveert_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_STATE], args, bool)
    cg.add(var.set_state(template_))


@automation.register_action(
    "MAX7219.turn_off", DisplayVisiblityAction, MAX7219_OFF_ACTION_SCHEMA
)
@automation.register_action(
    "MAX7219.turn_on", DisplayVisiblityAction, MAX7219_ON_ACTION_SCHEMA
)
async def MAX7219_visible_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_STATE], args, bool)
    cg.add(var.set_state(template_))


@automation.register_action(
    "MAX7219.reverse_off", DisplayReverseAction, MAX7219_OFF_ACTION_SCHEMA
)
@automation.register_action(
    "MAX7219.reverse_on", DisplayReverseAction, MAX7219_ON_ACTION_SCHEMA
)
async def MAX7219_reverse_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_STATE], args, bool)
    cg.add(var.set_state(template_))


MAX7219_INTENSITY_SCHEMA = automation.maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(MAX7219Component),
        cv.Optional(CONF_INTENSITY, default=15): cv.int_range(min=0, max=15),
    }
)


@automation.register_action(
    "MAX7219.intensity", DisplayIntensityAction, MAX7219_INTENSITY_SCHEMA
)
async def MAX7219_intensity_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_INTENSITY], args, cg.int8)
    cg.add(var.set_state(template_))
