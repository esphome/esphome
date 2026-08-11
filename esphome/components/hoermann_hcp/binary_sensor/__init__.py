import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_CONNECTIVITY, ENTITY_CATEGORY_DIAGNOSTIC
from esphome.types import ConfigType

from .. import CONF_HOERMANN_HCP_ID, HoermannHcp, hoermann_hcp_ns

DEPENDENCIES = ["hoermann_hcp"]

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
        }
    ),
    cv.has_at_least_one_key(CONF_IS_CONNECTED),
)


async def to_code(config: ConfigType) -> None:
    if (conf := config.get(CONF_IS_CONNECTED)) is not None:
        parent = await cg.get_variable(config[CONF_HOERMANN_HCP_ID])
        var = await binary_sensor.new_binary_sensor(conf, parent)
        await cg.register_component(var, conf)
