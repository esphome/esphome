import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import display, spi
from esphome.const import CONF_ID, CONF_INTENSITY, CONF_LAMBDA, CONF_NUM_CHIPS

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
    "MAX7219Component", cg.PollingComponent, spi.SPIDevice, display.DisplayBuffer
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
    await cg.register_component(var, config)
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
