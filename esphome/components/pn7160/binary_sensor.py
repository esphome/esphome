"""PN7160 binary sensor platform for ESPHome."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_ID, CONF_UID

from . import pn7160_ns, PN7160, CONF_PN7160_ID

DEPENDENCIES = ["pn7160"]

PN7160BinarySensor = pn7160_ns.class_(
    "PN7160BinarySensor", binary_sensor.BinarySensor
)


def validate_uid(value):
    """Accept 'xx-xx-xx-xx' or 'xx:xx:xx:xx' format UIDs."""
    value = cv.string(value).upper().replace(":", "-")
    parts = value.split("-")
    if not parts:
        raise cv.Invalid("UID must not be empty")
    for part in parts:
        if len(part) != 2:
            raise cv.Invalid(
                f"UID part '{part}' must be exactly 2 hex digits"
            )
        try:
            int(part, 16)
        except ValueError as e:
            raise cv.Invalid(
                f"UID part '{part}' is not valid hexadecimal"
            ) from e
    return value


CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(PN7160BinarySensor).extend(
    {
        cv.GenerateID(): cv.declare_id(PN7160BinarySensor),
        cv.GenerateID(CONF_PN7160_ID): cv.use_id(PN7160),
        cv.Required(CONF_UID): validate_uid,
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await binary_sensor.register_binary_sensor(var, config)

    parent = await cg.get_variable(config[CONF_PN7160_ID])
    cg.add(parent.register_tag_sensor(var))

    uid_str: str = config[CONF_UID]
    uid_bytes = [cg.uint8(int(p, 16)) for p in uid_str.split("-")]
    cg.add(var.set_uid(uid_bytes))
