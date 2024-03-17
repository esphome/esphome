import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import display, i2c
from esphome.const import (
    CONF_ID,
    CONF_INTENSITY,
    CONF_LAMBDA,
    CONF_RESET_PIN,
    CONF_NUM_CHIPS,
)

CODEOWNERS = ["@tylerwowen"]
DEPENDENCIES = ["i2c"]

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
CONF_BLINK_RATE = "blink_rate"

ht16k33_ns = cg.esphome_ns.namespace("ht16k33")
ChipLinesStyle = ht16k33_ns.enum("ChipLinesStyle")
CHIP_LINES_STYLE = {
    "ZIGZAG": ChipLinesStyle.ZIGZAG,
    "SNAKE": ChipLinesStyle.SNAKE,
}

ScrollMode = ht16k33_ns.enum("ScrollMode")
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

BLINK_RATES = {
    "OFF": 0,
    "2HZ": 1,
    "1HZ": 2,
    "0.5HZ": 3,
}

HT16K33Component = ht16k33_ns.class_(
    "HT16K33Component", i2c.I2CDevice, display.DisplayBuffer, cg.PollingComponent
)
HT16K33ComponentRef = HT16K33Component.operator("ref")

CONFIG_SCHEMA = (
    display.BASIC_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(HT16K33Component),
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_NUM_CHIPS, default=1): cv.int_range(min=1, max=8),
            cv.Optional(CONF_NUM_CHIP_LINES, default=1): cv.int_range(min=1, max=8),
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
            cv.Optional(CONF_BLINK_RATE, default="OFF"): cv.enum(
                BLINK_RATES, upper=True
            ),
        }
    )
    .extend(cv.polling_component_schema("1s"))
    .extend(i2c.i2c_device_schema(0x70))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await i2c.register_i2c_device(var, config)
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
    cg.add(var.set_blink_rate(config[CONF_BLINK_RATE]))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(HT16K33ComponentRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
    if CONF_RESET_PIN in config:
        reset = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(reset))
