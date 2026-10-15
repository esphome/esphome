from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INTERRUPT_PIN
from esphome.types import ConfigType

CODEOWNERS = ["@remcom"]
DEPENDENCIES = ["i2c"]
DOMAIN = "fusb302b"

CONF_REQUEST_VOLTAGE = "request_voltage"

fusb302b_ns = cg.esphome_ns.namespace("fusb302b")
FUSB302B = fusb302b_ns.class_("FUSB302B", cg.Component, i2c.I2CDevice)

# USB PD fixed supply voltages are 5, 9, 12, 15 and 20 V; the highest one that does not exceed the
# requested voltage is chosen.
validate_voltage = cv.int_range(min=5, max=20)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(FUSB302B),
            cv.Required(CONF_INTERRUPT_PIN): pins.internal_gpio_input_pullup_pin_schema,
            cv.Required(CONF_REQUEST_VOLTAGE): validate_voltage,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x22)),
    cv.only_on_esp32,
)


async def to_code(config: ConfigType) -> None:
    interrupt_pin = await cg.gpio_pin_expression(config[CONF_INTERRUPT_PIN])
    var = cg.new_Pvariable(config[CONF_ID], interrupt_pin)
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_request_voltage(config[CONF_REQUEST_VOLTAGE]))
