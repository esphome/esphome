from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c, touchscreen
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INTERRUPT_PIN, CONF_RESET_PIN
from esphome.types import ConfigType

from .. import gt9895_ns

GT9895Touchscreen = gt9895_ns.class_(
    "GT9895Touchscreen",
    touchscreen.Touchscreen,
    i2c.I2CDevice,
)

# The controller does not report its coordinate range, so the calibration is required
CONFIG_SCHEMA = (
    touchscreen.touchscreen_schema(calibration_required=True)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(GT9895Touchscreen),
            cv.Optional(CONF_INTERRUPT_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
        }
    )
    .extend(i2c.i2c_device_schema(0x5D))
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await touchscreen.register_touchscreen(var, config)
    await i2c.register_i2c_device(var, config)

    await cg.add_pin(var.set_interrupt_pin, config.get(CONF_INTERRUPT_PIN))
    await cg.add_pin(var.set_reset_pin, config.get(CONF_RESET_PIN))
