import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    STATE_CLASS_MEASUREMENT,
    UNIT_MICROSILVERTS_PER_HOUR,
    DEVICE_CLASS_EMPTY,
    ICON_RADIOACTIVE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    CONF_VERSION,
    CONF_STATUS,
    ICON_TIMER,
    UNIT_SECOND,
    STATE_CLASS_TOTAL_INCREASING,
    DEVICE_CLASS_DURATION,
)
from . import (
    CONF_GDK101_ID,
    CONF_RADIATION_DOSE_PER_1M,
    CONF_RADIATION_DOSE_PER_10M,
    CONF_MEAS_TIME,
    GDK101Component,
)

DEPENDENCIES = ["gdk101"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_GDK101_ID): cv.use_id(GDK101Component),
        cv.Required(CONF_RADIATION_DOSE_PER_1M): sensor.sensor_schema(
            icon=ICON_RADIOACTIVE,
            unit_of_measurement=UNIT_MICROSILVERTS_PER_HOUR,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_EMPTY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Required(CONF_RADIATION_DOSE_PER_10M): sensor.sensor_schema(
            icon=ICON_RADIOACTIVE,
            unit_of_measurement=UNIT_MICROSILVERTS_PER_HOUR,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_EMPTY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Required(CONF_VERSION): sensor.sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            accuracy_decimals=1,
        ),
        cv.Optional(CONF_STATUS): sensor.sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            accuracy_decimals=0,
        ),
        cv.Optional(CONF_MEAS_TIME): sensor.sensor_schema(
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

    sens = await sensor.new_sensor(config[CONF_RADIATION_DOSE_PER_1M])
    cg.add(hub.set_rad_1m_sensor(sens))

    sens = await sensor.new_sensor(config[CONF_RADIATION_DOSE_PER_10M])
    cg.add(hub.set_rad_10m_sensor(sens))

    sens = await sensor.new_sensor(config[CONF_VERSION])
    cg.add(hub.set_fw_version_sensor(sens))

    if CONF_STATUS in config:
        sens = await sensor.new_sensor(config[CONF_STATUS])
        cg.add(hub.set_status_sensor(sens))

    if CONF_MEAS_TIME in config:
        sens = await sensor.new_sensor(config[CONF_MEAS_TIME])
        cg.add(hub.set_meas_time_sensor(sens))
