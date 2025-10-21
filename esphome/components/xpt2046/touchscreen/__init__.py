from esphome import pins
import esphome.codegen as cg
from esphome.components import spi, touchscreen
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INTERRUPT_PIN, CONF_THRESHOLD

CODEOWNERS = ["@numo68", "@nielsnl68"]
DEPENDENCIES = ["spi"]

XPT2046_ns = cg.esphome_ns.namespace("xpt2046")
XPT2046Component = XPT2046_ns.class_(
    "XPT2046Component",
    touchscreen.Touchscreen,
    spi.SPIDevice,
)

CONFIG_SCHEMA = cv.All(
    touchscreen.touchscreen_schema(calibration_required=True)
    .extend(
        cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(XPT2046Component),
                cv.Optional(CONF_INTERRUPT_PIN): cv.All(
                    pins.internal_gpio_input_pin_schema
                ),
                cv.Optional(CONF_THRESHOLD, default=400): cv.int_range(min=0, max=4095),
            },
        )
    )
    .extend(spi.spi_device_schema()),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await spi.register_spi_device(var, config)
    await touchscreen.register_touchscreen(var, config)

    cg.add(var.set_threshold(config[CONF_THRESHOLD]))

    if CONF_INTERRUPT_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_INTERRUPT_PIN])
        cg.add(var.set_irq_pin(pin))
