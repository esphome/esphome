import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_VOLTAGE,
    CONF_BATTERY_LEVEL,
    CONF_SPEED,
    CONF_POSITION,
    DEVICE_CLASS_VOLTAGE,
    DEVICE_CLASS_BATTERY,
    STATE_CLASS_MEASUREMENT,
    UNIT_VOLT,
    UNIT_PERCENT,
    UNIT_REVOLUTIONS_PER_MINUTE,
    ICON_FLASH,
    ICON_BATTERY,
    ICON_ROTATE_RIGHT,
)
from . import FyrturMotorComponent, CONF_FYRTUR_MOTOR_ID

TYPES = [CONF_VOLTAGE, CONF_BATTERY_LEVEL, CONF_SPEED, CONF_POSITION]

ICON_ROLLER_SHADE = "mdi:roller-shade"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_FYRTUR_MOTOR_ID): cv.use_id(FyrturMotorComponent),
            cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                icon=ICON_FLASH,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                icon=ICON_BATTERY,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_BATTERY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_SPEED): sensor.sensor_schema(
                unit_of_measurement=UNIT_REVOLUTIONS_PER_MINUTE,
                icon=ICON_ROTATE_RIGHT,
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_POSITION): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                icon=ICON_ROLLER_SHADE,
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def setup_conf(config, key, hub):
    if sensor_config := config.get(key):
        sens = await sensor.new_sensor(sensor_config)
        cg.add(getattr(hub, f"set_{key}_sensor")(sens))


async def to_code(config):
    hub = await cg.get_variable(config[CONF_FYRTUR_MOTOR_ID])
    for key in TYPES:
        await setup_conf(config, key, hub)
