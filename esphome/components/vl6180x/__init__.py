from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ENABLE_PIN
from esphome.types import ConfigType

CODEOWNERS = ["@NonaSuomy"]
DEPENDENCIES = ["i2c"]
DOMAIN = "vl6180x"
MULTI_CONF = True

CONF_VL6180X_ID = "vl6180x_id"

vl6180x_ns = cg.esphome_ns.namespace("vl6180x")
VL6180XComponent = vl6180x_ns.class_(
    "VL6180XComponent", cg.PollingComponent, i2c.I2CDevice
)

_request_enable_pin_slot = cg.slot_counter("VL6180X_ENABLE_PIN_COUNT")


def _validate_address(config: ConfigType) -> ConfigType:
    # The sensor always boots at 0x29; only an enable pin lets it move elsewhere
    if config[CONF_ADDRESS] != 0x29 and CONF_ENABLE_PIN not in config:
        raise cv.Invalid(
            "An address other than 0x29 needs an enable_pin; give every VL6180X "
            "on the bus an enable_pin"
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(VL6180XComponent),
            cv.Optional(CONF_ENABLE_PIN): pins.gpio_output_pin_schema,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x29)),
    _validate_address,
)


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)
    if pin := await cg.add_pin(var.set_enable_pin, config.get(CONF_ENABLE_PIN)):
        _request_enable_pin_slot()
        cg.add(var.register_enable_pin(pin))
