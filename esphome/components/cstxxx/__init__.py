from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c, touchscreen
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INTERRUPT_PIN, CONF_RESET_PIN
from esphome.cpp_generator import MockObj, MockObjClass
from esphome.types import ConfigType

CODEOWNERS = ["@clydebarrow", "@latonita"]
DEPENDENCIES = ["i2c", "touchscreen"]
DOMAIN = "cstxxx"

cstxxx_ns = cg.esphome_ns.namespace("cstxxx")
CSTTouchscreen = cstxxx_ns.class_(
    "CSTTouchscreen", touchscreen.Touchscreen, i2c.I2CDevice
)


def cst_touchscreen_schema(touchscreen_class: MockObjClass, address: int) -> cv.Schema:
    """Return the touchscreen schema of a Hynitron driver."""
    return (
        touchscreen.touchscreen_schema("100ms")
        .extend(
            {
                cv.GenerateID(): cv.declare_id(touchscreen_class),
                cv.Optional(CONF_INTERRUPT_PIN): pins.internal_gpio_input_pin_schema,
                cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            }
        )
        .extend(i2c.i2c_device_schema(address))
    )


async def new_cst_touchscreen(config: ConfigType) -> MockObj:
    """Create a Hynitron touchscreen and set its pins."""
    var = cg.new_Pvariable(config[CONF_ID])
    await touchscreen.register_touchscreen(var, config)
    await i2c.register_i2c_device(var, config)

    await cg.add_pin(var.set_interrupt_pin, config.get(CONF_INTERRUPT_PIN))
    await cg.add_pin(var.set_reset_pin, config.get(CONF_RESET_PIN))
    return var
