from esphome import automation
import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_SOURCE_ID, CONF_TYPE
from esphome.types import ConfigType

from .. import BTHomeRemoteObject, RemoteDevice, add_handler, client_ns
from ..bthome import bthome_object_type_validator, bthome_object_types
from .object_types import TEXT_SENSOR_OBJECT_TYPES

CODEOWNERS = ["@jpeletier"]

DEPENDENCIES = ["bthome", "text_sensor"]

CONF_ON_RAW_DATA = "on_raw_data"

BTHomeTextSensor = client_ns.class_(
    "BTHomeTextSensor", BTHomeRemoteObject, text_sensor.TextSensor
)

_RAW_DATA_ARGS = [(cg.std_span.template(cg.uint8.operator("const")), "x")]


def _validate_raw_data_trigger(config: ConfigType) -> ConfigType:
    if CONF_ON_RAW_DATA in config and config[CONF_TYPE] != "RAW":
        raise cv.Invalid(f"'{CONF_ON_RAW_DATA}' is only available with type RAW")
    return config


CONFIG_SCHEMA = cv.All(
    text_sensor.text_sensor_schema(class_=BTHomeTextSensor).extend(
        {
            cv.Required(CONF_SOURCE_ID): cv.use_id(RemoteDevice),
            cv.Required(CONF_TYPE): bthome_object_type_validator(
                TEXT_SENSOR_OBJECT_TYPES
            ),
            cv.Optional(CONF_ON_RAW_DATA): automation.validate_automation({}),
        }
    ),
    _validate_raw_data_trigger,
)


async def to_code(config: ConfigType) -> None:
    var = await text_sensor.new_text_sensor(config)
    object_type_key = config[CONF_TYPE]
    cg.add(var.set_object_type(getattr(bthome_object_types, object_type_key)))
    add_handler(
        var,
        config[CONF_SOURCE_ID],
        TEXT_SENSOR_OBJECT_TYPES[object_type_key].object_id,
    )
    for conf in config.get(CONF_ON_RAW_DATA, []):
        await automation.build_callback_automation(
            var, "add_on_raw_data_callback", _RAW_DATA_ARGS, conf
        )
