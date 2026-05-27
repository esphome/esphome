import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv

from .. import CONF_C4004_ID, C4004Component

CONF_PEOPLE_COUNT = "people_count"
CONF_MOTION_STATE = "motion_state"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_C4004_ID): cv.use_id(C4004Component),
        cv.Optional(CONF_PEOPLE_COUNT): sensor.sensor_schema(
            icon="mdi:account-group",
            accuracy_decimals=0,
            unit_of_measurement="people",
        ),
        cv.Optional(CONF_MOTION_STATE): sensor.sensor_schema(
            icon="mdi:run",
            accuracy_decimals=0,
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_C4004_ID])

    if people_count_config := config.get(CONF_PEOPLE_COUNT):
        sens = await sensor.new_sensor(people_count_config)
        cg.add(parent.set_people_count_sensor(sens))

    if motion_state_config := config.get(CONF_MOTION_STATE):
        sens = await sensor.new_sensor(motion_state_config)
        cg.add(parent.set_motion_state_sensor(sens))
