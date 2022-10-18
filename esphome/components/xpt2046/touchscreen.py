import esphome.codegen as cg
import esphome.config_validation as cv

from esphome import pins
from esphome.components import spi, touchscreen
from esphome.const import CONF_ID, CONF_THRESHOLD

CODEOWNERS = ["@numo68", "@nielsnl68"]
DEPENDENCIES = ["spi"]

XPT2046_ns = cg.esphome_ns.namespace("xpt2046")
XPT2046Component = XPT2046_ns.class_(
    "XPT2046Component", touchscreen.Touchscreen, cg.PollingComponent, spi.SPIDevice
)

CONF_INTERRUPT_PIN = "interrupt_pin"

CONF_REPORT_INTERVAL = "report_interval"
CONF_CALIBRATION_X_MIN = "calibration_x_min"
CONF_CALIBRATION_X_MAX = "calibration_x_max"
CONF_CALIBRATION_Y_MIN = "calibration_y_min"
CONF_CALIBRATION_Y_MAX = "calibration_y_max"
CONF_SWAP_X_Y = "swap_x_y"

# obsolete Keys
CONF_DIMENSION_X = "dimension_x"
CONF_DIMENSION_Y = "dimension_y"
CONF_IRQ_PIN = "irq_pin"


def validate_xpt2046(config):
    if (
        abs(
            cv.int_(config[CONF_CALIBRATION_X_MAX])
            - cv.int_(config[CONF_CALIBRATION_X_MIN])
        )
        < 1000
    ):
        raise cv.Invalid("Calibration X values difference < 1000")

    if (
        abs(
            cv.int_(config[CONF_CALIBRATION_Y_MAX])
            - cv.int_(config[CONF_CALIBRATION_Y_MIN])
        )
        < 1000
    ):
        raise cv.Invalid("Calibration Y values difference < 1000")

    return config


def report_interval(value):
    if value == "never":
        return 4294967295  # uint32_t max
    return cv.positive_time_period_milliseconds(value)


CONFIG_SCHEMA = touchscreen.TOUCHSCREEN_SCHEMA.extend(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(XPT2046Component),
            cv.Optional(CONF_INTERRUPT_PIN): cv.All(
                pins.internal_gpio_input_pin_schema
            ),
            cv.Optional(CONF_CALIBRATION_X_MIN, default=0): cv.int_range(
                min=0, max=4095
            ),
            cv.Optional(CONF_CALIBRATION_X_MAX, default=4095): cv.int_range(
                min=0, max=4095
            ),
            cv.Optional(CONF_CALIBRATION_Y_MIN, default=0): cv.int_range(
                min=0, max=4095
            ),
            cv.Optional(CONF_CALIBRATION_Y_MAX, default=4095): cv.int_range(
                min=0, max=4095
            ),
            cv.Optional(CONF_THRESHOLD, default=400): cv.int_range(min=0, max=4095),
            cv.Optional(CONF_REPORT_INTERVAL, default="never"): report_interval,
            cv.Optional(CONF_SWAP_X_Y, default=False): cv.boolean,
            # obsolete Keys
            cv.Optional(CONF_IRQ_PIN): cv.invalid("Rename IRQ_PIN to INTERUPT_PIN"),
            cv.Optional(CONF_DIMENSION_X): cv.invalid(
                "This key is now obsolete, please remove it"
            ),
            cv.Optional(CONF_DIMENSION_Y): cv.invalid(
                "This key is now obsolete, please remove it"
            ),
        },
    )
    .extend(cv.polling_component_schema("50ms"))
    .extend(spi.spi_device_schema()),
).add_extra(validate_xpt2046)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)
    await touchscreen.register_touchscreen(var, config)

    cg.add(var.set_threshold(config[CONF_THRESHOLD]))
    cg.add(var.set_report_interval(config[CONF_REPORT_INTERVAL]))
    cg.add(var.set_swap_x_y(config[CONF_SWAP_X_Y]))
    cg.add(
        var.set_calibration(
            config[CONF_CALIBRATION_X_MIN],
            config[CONF_CALIBRATION_X_MAX],
            config[CONF_CALIBRATION_Y_MIN],
            config[CONF_CALIBRATION_Y_MAX],
        )
    )

    if CONF_INTERRUPT_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_INTERRUPT_PIN])
        cg.add(var.set_irq_pin(pin))
