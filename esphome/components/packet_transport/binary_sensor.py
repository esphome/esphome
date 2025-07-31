import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_DATA,
    CONF_ID,
    CONF_NAME,
    CONF_STATUS,
    CONF_TYPE,
    DEVICE_CLASS_CONNECTIVITY,
    ENTITY_CATEGORY_DIAGNOSTIC,
)
import esphome.final_validate as fv

from . import (
    CONF_ENCRYPTION,
    CONF_PING_PONG_ENABLE,
    CONF_PROVIDER,
    CONF_PROVIDERS,
    CONF_REMOTE_ID,
    CONF_TRANSPORT_ID,
    PacketTransport,
    packet_transport_sensor_schema,
    provider_name_validate,
)

STATUS_SENSOR_SCHEMA = binary_sensor.binary_sensor_schema(
    device_class=DEVICE_CLASS_CONNECTIVITY,
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
).extend(
    {
        cv.GenerateID(CONF_TRANSPORT_ID): cv.use_id(PacketTransport),
        cv.Required(CONF_PROVIDER): provider_name_validate,
    }
)

CONFIG_SCHEMA = cv.typed_schema(
    {
        CONF_DATA: packet_transport_sensor_schema(binary_sensor.binary_sensor_schema()),
        CONF_STATUS: STATUS_SENSOR_SCHEMA,
    },
    key=CONF_TYPE,
    default_type=CONF_DATA,
)


def _final_validate(config):
    if config[CONF_TYPE] != CONF_STATUS:
        # Only run this validation if a status sensor is being configured
        return config
    full_config = fv.full_config.get()
    transport_path = full_config.get_path_for_id(config[CONF_TRANSPORT_ID])[:-1]
    transport_config = full_config.get_config_for_path(transport_path)
    if transport_config[CONF_PING_PONG_ENABLE] and any(
        CONF_ENCRYPTION in p
        for p in transport_config[CONF_PROVIDERS]
        if p[CONF_NAME] == config[CONF_PROVIDER]
    ):
        return config
    raise cv.Invalid(
        "Status sensor requires ping-pong to be enabled and the nominated provider to use encryption."
    )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    comp = await cg.get_variable(config[CONF_TRANSPORT_ID])
    if config[CONF_TYPE] == CONF_STATUS:
        cg.add(comp.set_provider_status_sensor(config[CONF_PROVIDER], var))
        cg.add_define("USE_STATUS_SENSOR")
    else:  # CONF_DATA is default
        remote_id = str(config.get(CONF_REMOTE_ID) or config.get(CONF_ID))
        cg.add(comp.add_remote_binary_sensor(config[CONF_PROVIDER], remote_id, var))
