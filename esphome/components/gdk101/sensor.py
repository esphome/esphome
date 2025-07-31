import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_MEASUREMENT_DURATION,
    CONF_STATUS,
    CONF_VERSION,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_EMPTY,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_RADIOACTIVE,
    ICON_TIMER,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_MICROSILVERTS_PER_HOUR,
    UNIT_SECOND,
)

from . import CONF_GDK101_ID, GDK101Component

CONF_RADIATION_DOSE_PER_1M = "radiation_dose_per_1m"
CONF_RADIATION_DOSE_PER_10M = "radiation_dose_per_10m"

DEPENDENCIES = ["gdk101"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_GDK101_ID): cv.use_id(GDK101Component),
        cv.Optional(CONF_RADIATION_DOSE_PER_1M): sensor.sensor_schema(
            icon=ICON_RADIOACTIVE,
            unit_of_measurement=UNIT_MICROSILVERTS_PER_HOUR,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_EMPTY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_RADIATION_DOSE_PER_10M): sensor.sensor_schema(
            icon=ICON_RADIOACTIVE,
            unit_of_measurement=UNIT_MICROSILVERTS_PER_HOUR,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_EMPTY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_VERSION): sensor.sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            accuracy_decimals=1,
        ),
        cv.Optional(CONF_STATUS): sensor.sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            accuracy_decimals=0,
        ),
        cv.Optional(CONF_MEASUREMENT_DURATION): sensor.sensor_schema(
            unit_of_measurement=UNIT_SECOND,
            icon=ICON_TIMER,
            accuracy_decimals=0,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            device_class=DEVICE_CLASS_DURATION,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_GDK101_ID])

    if radiation_dose_per_1m := config.get(CONF_RADIATION_DOSE_PER_1M):
        sens = await sensor.new_sensor(radiation_dose_per_1m)
        cg.add(hub.set_rad_1m_sensor(sens))

    if radiation_dose_per_10m := config.get(CONF_RADIATION_DOSE_PER_10M):
        sens = await sensor.new_sensor(radiation_dose_per_10m)
        cg.add(hub.set_rad_10m_sensor(sens))

    if version_config := config.get(CONF_VERSION):
        sens = await sensor.new_sensor(version_config)
        cg.add(hub.set_fw_version_sensor(sens))

    if status_config := config.get(CONF_STATUS):
        sens = await sensor.new_sensor(status_config)
        cg.add(hub.set_status_sensor(sens))

    if measurement_duration_config := config.get(CONF_MEASUREMENT_DURATION):
        sens = await sensor.new_sensor(measurement_duration_config)
        cg.add(hub.set_measurement_duration_sensor(sens))
