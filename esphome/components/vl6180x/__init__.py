from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.i2c import CONF_I2C_ID
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ENABLE_PIN
import esphome.final_validate as fv
from esphome.types import ConfigType

AUTO_LOAD = ["sensor"]
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

DEFAULT_ADDRESS = 0x29


def _validate_address(config: ConfigType) -> ConfigType:
    # The sensor always boots at 0x29; only an enable pin lets it move elsewhere
    if config[CONF_ADDRESS] != DEFAULT_ADDRESS and CONF_ENABLE_PIN not in config:
        raise cv.Invalid(
            f"An address other than 0x{DEFAULT_ADDRESS:02X} needs an enable_pin"
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
    .extend(i2c.i2c_device_schema(DEFAULT_ADDRESS)),
    _validate_address,
)


def _final_validate(config: ConfigType) -> ConfigType:
    # With several sensors on one bus each needs its own enable_pin so it can be held in reset
    # while the others are re-addressed, and none may stay at 0x29, or a later sibling boots at
    # the same address and the re-address write reaches both chips.
    addresses = [
        conf[CONF_ADDRESS]
        for conf in fv.full_config.get()[DOMAIN]
        if conf[CONF_I2C_ID] == config[CONF_I2C_ID]
    ]
    if len(addresses) < 2:
        return config
    if CONF_ENABLE_PIN not in config:
        raise cv.Invalid(
            "Every VL6180X on a shared I2C bus needs an enable_pin",
            path=[CONF_ENABLE_PIN],
        )
    address = config[CONF_ADDRESS]
    if address == DEFAULT_ADDRESS:
        raise cv.Invalid(
            f"VL6180X sensors sharing an I2C bus must all be re-addressed away "
            f"from 0x{DEFAULT_ADDRESS:02X}",
            path=[CONF_ADDRESS],
        )
    if addresses.count(address) > 1:
        raise cv.Invalid(
            f"Address 0x{address:02X} is used by more than one VL6180X on this bus",
            path=[CONF_ADDRESS],
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)
    if pin := await cg.add_pin(var.set_enable_pin, config.get(CONF_ENABLE_PIN)):
        _request_enable_pin_slot()
        cg.add(var.register_enable_pin(pin))
