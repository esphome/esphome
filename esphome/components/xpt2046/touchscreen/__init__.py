import esphome.codegen as cg
import esphome.config_validation as cv

from esphome import pins
from esphome.components import spi, touchscreen
from esphome.const import CONF_ID, CONF_THRESHOLD, CONF_INTERRUPT_PIN

CODEOWNERS = ["@numo68", "@nielsnl68"]
DEPENDENCIES = ["spi"]

XPT2046_ns = cg.esphome_ns.namespace("xpt2046")
XPT2046Component = XPT2046_ns.class_(
    "XPT2046Component",
    touchscreen.Touchscreen,
    spi.SPIDevice,
)

CONF_CALIBRATION_X_MIN = "calibration_x_min"
CONF_CALIBRATION_X_MAX = "calibration_x_max"
CONF_CALIBRATION_Y_MIN = "calibration_y_min"
CONF_CALIBRATION_Y_MAX = "calibration_y_max"

CONFIG_SCHEMA = cv.All(
    touchscreen.TOUCHSCREEN_SCHEMA.extend(
        cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(XPT2046Component),
                cv.Optional(CONF_INTERRUPT_PIN): cv.All(
                    pins.internal_gpio_input_pin_schema
                ),
                cv.Optional(CONF_THRESHOLD, default=400): cv.int_range(min=0, max=4095),
                cv.Optional(
                    touchscreen.CONF_CALIBRATION
                ): touchscreen.calibration_schema(4095),
                cv.Optional(CONF_CALIBRATION_X_MIN): cv.invalid(
                    "Deprecated: use the new 'calibration' configuration variable"
                ),
                cv.Optional(CONF_CALIBRATION_X_MAX): cv.invalid(
                    "Deprecated: use the new 'calibration' configuration variable"
                ),
                cv.Optional(CONF_CALIBRATION_Y_MIN): cv.invalid(
                    "Deprecated: use the new 'calibration' configuration variable"
                ),
                cv.Optional(CONF_CALIBRATION_Y_MAX): cv.invalid(
                    "Deprecated: use the new 'calibration' configuration variable"
                ),
                cv.Optional(CONF_CALIBRATION_Y_MAX): cv.invalid(
                    "Deprecated: use the new 'calibration' configuration variable"
                ),
                cv.Optional("report_interval"): cv.invalid(
                    "Deprecated: use the 'update_interval' configuration variable"
                ),
            },
        )
    ).extend(spi.spi_device_schema()),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await spi.register_spi_device(var, config)
    await touchscreen.register_touchscreen(var, config)

    cg.add(var.set_threshold(config[CONF_THRESHOLD]))

    if CONF_INTERRUPT_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_INTERRUPT_PIN])
        cg.add(var.set_irq_pin(pin))
