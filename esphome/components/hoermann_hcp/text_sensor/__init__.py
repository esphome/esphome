import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_VERSION, ENTITY_CATEGORY_DIAGNOSTIC, ICON_CHIP
from esphome.types import ConfigType

from .. import CONF_HOERMANN_HCP_ID, HoermannHcp, hoermann_hcp_ns

DEPENDENCIES = ["hoermann_hcp"]

CONF_DOOR_STATE = "door_state"
CONF_SERIAL_NUMBER = "serial_number"

HoermannHcpDoorStateTextSensor = hoermann_hcp_ns.class_(
    "HoermannHcpDoorStateTextSensor", text_sensor.TextSensor, cg.Component
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_HOERMANN_HCP_ID): cv.use_id(HoermannHcp),
            cv.Optional(CONF_DOOR_STATE): text_sensor.text_sensor_schema(
                HoermannHcpDoorStateTextSensor, icon="mdi:garage"
            ).extend(cv.COMPONENT_SCHEMA),
            cv.Optional(CONF_SERIAL_NUMBER): text_sensor.text_sensor_schema(
                icon="mdi:data-matrix", entity_category=ENTITY_CATEGORY_DIAGNOSTIC
            ),
            cv.Optional(CONF_VERSION): text_sensor.text_sensor_schema(
                icon=ICON_CHIP, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
            ),
        }
    ),
    cv.has_at_least_one_key(CONF_DOOR_STATE, CONF_SERIAL_NUMBER, CONF_VERSION),
)


async def to_code(config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_HOERMANN_HCP_ID])
    if (conf := config.get(CONF_DOOR_STATE)) is not None:
        var = await text_sensor.new_text_sensor(conf, parent)
        await cg.register_component(var, conf)
    # Only the identity sensors need the exchange with the motor compiled in.
    if CONF_SERIAL_NUMBER in config or CONF_VERSION in config:
        cg.add_define("USE_HOERMANN_HCP_IDENTITY")
    if (conf := config.get(CONF_SERIAL_NUMBER)) is not None:
        sens = await text_sensor.new_text_sensor(conf)
        cg.add(parent.set_serial_number_text_sensor(sens))
    if (conf := config.get(CONF_VERSION)) is not None:
        sens = await text_sensor.new_text_sensor(conf)
        cg.add(parent.set_version_text_sensor(sens))
