import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_UID, CONF_ID
from esphome.core import HexInt
from . import rc522_ns, RC522, CONF_RC522_ID

DEPENDENCIES = ["rc522"]


def validate_uid(value):
    value = cv.string_strict(value)
    for x in value.split("-"):
        if len(x) != 2:
            raise cv.Invalid(
                "Each part (separated by '-') of the UID must be two characters "
                "long."
            )
        try:
            x = int(x, 16)
        except ValueError as err:
            raise cv.Invalid(
                "Valid characters for parts of a UID are 0123456789ABCDEF."
            ) from err
        if x < 0 or x > 255:
            raise cv.Invalid(
                "Valid values for UID parts (separated by '-') are 00 to FF"
            )
    return value


RC522BinarySensor = rc522_ns.class_("RC522BinarySensor", binary_sensor.BinarySensor)

CONFIG_SCHEMA = binary_sensor.BINARY_SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(RC522BinarySensor),
        cv.GenerateID(CONF_RC522_ID): cv.use_id(RC522),
        cv.Required(CONF_UID): validate_uid,
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await binary_sensor.register_binary_sensor(var, config)

    hub = await cg.get_variable(config[CONF_RC522_ID])
    cg.add(hub.register_tag(var))
    addr = [HexInt(int(x, 16)) for x in config[CONF_UID].split("-")]
    cg.add(var.set_uid(addr))
