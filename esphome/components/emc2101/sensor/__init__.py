import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_EXTERNAL_TEMPERATURE,
    CONF_ID,
    CONF_INTERNAL_TEMPERATURE,
    CONF_SPEED,
    DEVICE_CLASS_TEMPERATURE,
    ICON_PERCENT,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    UNIT_REVOLUTIONS_PER_MINUTE,
)
from esphome.types import ConfigType

from .. import CONF_EMC2101_ID, EMC2101_COMPONENT_SCHEMA, emc2101_ns

DEPENDENCIES = ["emc2101"]

CONF_DUTY_CYCLE = "duty_cycle"

EMC2101Sensor = emc2101_ns.class_("EMC2101Sensor", cg.PollingComponent)

CONFIG_SCHEMA = EMC2101_COMPONENT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(EMC2101Sensor),
        cv.Optional(CONF_INTERNAL_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_EXTERNAL_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_SPEED): sensor.sensor_schema(
            unit_of_measurement=UNIT_REVOLUTIONS_PER_MINUTE,
            accuracy_decimals=2,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:fan",
        ),
        cv.Optional(CONF_DUTY_CYCLE): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=2,
            state_class=STATE_CLASS_MEASUREMENT,
            icon=ICON_PERCENT,
        ),
    }
).extend(cv.polling_component_schema("60s"))


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_EMC2101_ID])
    var = cg.new_Pvariable(config[CONF_ID], hub)
    await cg.register_component(var, config)

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_INTERNAL_TEMPERATURE, var.set_internal_temperature_sensor)
    await sensors(CONF_EXTERNAL_TEMPERATURE, var.set_external_temperature_sensor)
    await sensors(CONF_SPEED, var.set_speed_sensor)
    await sensors(CONF_DUTY_CYCLE, var.set_duty_cycle_sensor)
