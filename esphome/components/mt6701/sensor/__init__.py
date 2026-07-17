import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    ICON_ROTATE_RIGHT,
    STATE_CLASS_MEASUREMENT,
    UNIT_DEGREES,
    UNIT_EMPTY,
)

from .. import CONF_MT6701_ID, MT6701Component, mt6701_ns

AUTO_LOAD = ["mt6701"]

MT6701Sensor = mt6701_ns.class_("MT6701Sensor", sensor.Sensor, cg.PollingComponent)

CONF_RAW_COUNT = "raw_count"

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        MT6701Sensor,
        unit_of_measurement=UNIT_DEGREES,
        accuracy_decimals=2,
        icon=ICON_ROTATE_RIGHT,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.GenerateID(CONF_MT6701_ID): cv.use_id(MT6701Component),
            cv.Optional(CONF_RAW_COUNT): sensor.sensor_schema(
                unit_of_measurement=UNIT_EMPTY,
                accuracy_decimals=0,
                icon=ICON_ROTATE_RIGHT,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_parented(var, config[CONF_MT6701_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    if raw_count_config := config.get(CONF_RAW_COUNT):
        sens = await sensor.new_sensor(raw_count_config)
        cg.add(var.set_raw_count_sensor(sens))
