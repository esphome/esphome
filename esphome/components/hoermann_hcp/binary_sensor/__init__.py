import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_CONNECTIVITY,
    DEVICE_CLASS_PROBLEM,
    ENTITY_CATEGORY_DIAGNOSTIC,
)
from esphome.types import ConfigType

from .. import CONF_HOERMANN_HCP_ID, HoermannHcp, hoermann_hcp_ns

DEPENDENCIES = ["hoermann_hcp"]

CONF_ACTUATOR_ERROR = "actuator_error"
CONF_IS_CONNECTED = "is_connected"

HoermannHcpConnectedBinarySensor = hoermann_hcp_ns.class_(
    "HoermannHcpConnectedBinarySensor", binary_sensor.BinarySensor, cg.Component
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_HOERMANN_HCP_ID): cv.use_id(HoermannHcp),
            cv.Optional(CONF_IS_CONNECTED): binary_sensor.binary_sensor_schema(
                HoermannHcpConnectedBinarySensor,
                device_class=DEVICE_CLASS_CONNECTIVITY,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ).extend(cv.COMPONENT_SCHEMA),
            cv.Optional(CONF_ACTUATOR_ERROR): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_PROBLEM,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        }
    ),
    cv.has_at_least_one_key(CONF_IS_CONNECTED, CONF_ACTUATOR_ERROR),
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_HOERMANN_HCP_ID])
    if (conf := config.get(CONF_IS_CONNECTED)) is not None:
        var = await binary_sensor.new_binary_sensor(conf, hub)
        await cg.register_component(var, conf)
    binary_sensors = binary_sensor.sub_binary_sensors(config)
    await binary_sensors(CONF_ACTUATOR_ERROR, hub.set_actuator_error_binary_sensor)
