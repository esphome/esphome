import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ACCELERATION_X,
    CONF_ACCELERATION_Y,
    CONF_ACCELERATION_Z,
    CONF_NAME,
    ICON_BRIEFCASE_DOWNLOAD,
    STATE_CLASS_MEASUREMENT,
    UNIT_METER_PER_SECOND_SQUARED,
)
from . import MSA3xxComponent, CONF_MSA3XX_ID

CODEOWNERS = ["@latonita"]
DEPENDENCIES = ["msa3xx"]


accel_schema = cv.maybe_simple_value(
    sensor.sensor_schema(
        unit_of_measurement=UNIT_METER_PER_SECOND_SQUARED,
        icon=ICON_BRIEFCASE_DOWNLOAD,
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    key=CONF_NAME,
)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_MSA3XX_ID): cv.use_id(MSA3xxComponent),
            cv.Optional(CONF_ACCELERATION_X): accel_schema,
            cv.Optional(CONF_ACCELERATION_Y): accel_schema,
            cv.Optional(CONF_ACCELERATION_Z): accel_schema,
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_MSA3XX_ID])

    for d in ["x", "y", "z"]:
        accel_key = f"acceleration_{d}"
        if accel_key in config:
            sens = await sensor.new_sensor(config[accel_key])
            cg.add(getattr(hub, f"set_acceleration_{d}_sensor")(sens))
