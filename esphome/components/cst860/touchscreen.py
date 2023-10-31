import esphome.codegen as cg
import esphome.config_validation as cv

from esphome import pins
from esphome.components import touchscreen, i2c
from esphome.const import CONF_ID, CONF_INTERRUPT_PIN

CODEOWNERS = ["@dirkjankrijnders"]
DEPENDENCIES = ["i2c"]

CST860_ns = cg.esphome_ns.namespace("cst860")
CST860Component = CST860_ns.class_(
    "CST860Component",
    touchscreen.Touchscreen,
    cg.PollingComponent,
    i2c.I2CDevice,
)

CONF_REPORT_INTERVAL = "report_interval"
CONF_X_MAX = "x_max"
CONF_Y_MAX = "y_max"
CONF_INVERT_X = "invert_x"
CONF_INVERT_Y = "invert_y"
CONF_SWAP_X_Y = "swap_x_y"


def validate_cst860(config):
    return config


def report_interval(value):
    if value == "never":
        return 4294967295  # uint32_t max
    return cv.positive_time_period_milliseconds(value)


CONFIG_SCHEMA = touchscreen.TOUCHSCREEN_SCHEMA.extend(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CST860Component),
            cv.Optional(CONF_INTERRUPT_PIN): cv.All(
                pins.internal_gpio_input_pin_schema
            ),
            cv.Optional(CONF_X_MAX, default=4095): cv.int_range(min=0, max=4095),
            cv.Optional(CONF_Y_MAX, default=4095): cv.int_range(min=0, max=4095),
            cv.Optional(CONF_REPORT_INTERVAL, default="never"): report_interval,
            cv.Optional(CONF_INVERT_X, default=False): cv.boolean,
            cv.Optional(CONF_INVERT_Y, default=False): cv.boolean,
            cv.Optional(CONF_SWAP_X_Y, default=False): cv.boolean,
        },
    )
    .extend(cv.polling_component_schema("50ms"))
    .extend(i2c.i2c_device_schema(0x15)),
).add_extra(validate_cst860)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    await touchscreen.register_touchscreen(var, config)

    cg.add(var.set_report_interval(config[CONF_REPORT_INTERVAL]))
    cg.add(var.set_swap_x_y(config[CONF_SWAP_X_Y]))
    cg.add(
        var.set_calibration(
            config[CONF_X_MAX],
            config[CONF_Y_MAX],
            config[CONF_INVERT_X],
            config[CONF_INVERT_Y],
        )
    )

    if CONF_INTERRUPT_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_INTERRUPT_PIN])
        cg.add(var.set_irq_pin(pin))
