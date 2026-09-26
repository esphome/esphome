import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_FREQUENCY,
    DEVICE_CLASS_FREQUENCY,
    ICON_ROTATE_RIGHT,
    STATE_CLASS_MEASUREMENT,
    UNIT_HERTZ,
)

from .. import CONF_M5_UNIT_BLDC_ID, UNIT_RPM, M5UnitBldc

CONF_RPM = "rpm"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_M5_UNIT_BLDC_ID): cv.use_id(M5UnitBldc),
    cv.Optional(CONF_RPM): sensor.sensor_schema(
        unit_of_measurement=UNIT_RPM,
        icon=ICON_ROTATE_RIGHT,
        accuracy_decimals=1,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_FREQUENCY): sensor.sensor_schema(
        unit_of_measurement=UNIT_HERTZ,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_FREQUENCY,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
}


async def to_code(config):
    parent = await cg.get_variable(config[CONF_M5_UNIT_BLDC_ID])

    if rpm_config := config.get(CONF_RPM):
        sens = await sensor.new_sensor(rpm_config)
        cg.add(parent.set_rpm_sensor(sens))

    if frequency_config := config.get(CONF_FREQUENCY):
        sens = await sensor.new_sensor(frequency_config)
        cg.add(parent.set_frequency_sensor(sens))
