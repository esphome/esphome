from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c, touchscreen
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ID, CONF_INTERRUPT_PIN, CONF_RESET_PIN
from esphome.types import ConfigType

from .. import gt911_ns

GT911ButtonListener = gt911_ns.class_("GT911ButtonListener")
GT911Touchscreen = gt911_ns.class_(
    "GT911Touchscreen",
    touchscreen.Touchscreen,
    i2c.I2CDevice,
)

# The reset sequence drives the interrupt pin to select one of the two addresses the chip offers
ADDRESSES = (0x5D, 0x14)

CONFIG_SCHEMA = cv.All(
    touchscreen.TOUCHSCREEN_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(GT911Touchscreen),
            cv.Optional(CONF_INTERRUPT_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
        }
    ).extend(i2c.i2c_device_schema(0x5D)),
    cv.Schema(
        {cv.Required(CONF_ADDRESS): cv.one_of(*ADDRESSES, int=True)},
        extra=cv.ALLOW_EXTRA,
    ),
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await touchscreen.register_touchscreen(var, config)
    await i2c.register_i2c_device(var, config)

    if interrupt_pin := config.get(CONF_INTERRUPT_PIN):
        cg.add(var.set_interrupt_pin(await cg.gpio_pin_expression(interrupt_pin)))
    if reset_pin := config.get(CONF_RESET_PIN):
        cg.add(var.set_reset_pin(await cg.gpio_pin_expression(reset_pin)))
