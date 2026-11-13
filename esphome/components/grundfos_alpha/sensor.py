import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_CURRENT,
    CONF_FLOW,
    CONF_HEAD,
    CONF_POWER,
    CONF_SPEED,
    CONF_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    DEVICE_CLASS_VOLUME_FLOW_RATE,
    STATE_CLASS_MEASUREMENT,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_CUBIC_METER_PER_HOUR,
    UNIT_METER,
    UNIT_REVOLUTIONS_PER_MINUTE,
    UNIT_VOLT,
    UNIT_WATT,
)
from esphome.types import ConfigType

from . import CONF_GRUNDFOS_ALPHA_ID, GrundfosAlpha

DEPENDENCIES = ["grundfos_alpha"]

CONF_MEDIUM_TEMPERATURE = "medium_temperature"


def _schema(unit: str, decimals: int, device_class: str | None = None) -> cv.Schema:
    return sensor.sensor_schema(
        unit_of_measurement=unit,
        accuracy_decimals=decimals,
        device_class=device_class or cv.UNDEFINED,
        state_class=STATE_CLASS_MEASUREMENT,
    )


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_GRUNDFOS_ALPHA_ID): cv.use_id(GrundfosAlpha),
        cv.Optional(CONF_FLOW): _schema(
            UNIT_CUBIC_METER_PER_HOUR, 3, DEVICE_CLASS_VOLUME_FLOW_RATE
        ),
        cv.Optional(CONF_HEAD): _schema(UNIT_METER, 2),
        cv.Optional(CONF_MEDIUM_TEMPERATURE): _schema(
            UNIT_CELSIUS, 1, DEVICE_CLASS_TEMPERATURE
        ),
        cv.Optional(CONF_VOLTAGE): _schema(UNIT_VOLT, 1, DEVICE_CLASS_VOLTAGE),
        cv.Optional(CONF_CURRENT): _schema(UNIT_AMPERE, 3, DEVICE_CLASS_CURRENT),
        cv.Optional(CONF_POWER): _schema(UNIT_WATT, 1, DEVICE_CLASS_POWER),
        cv.Optional(CONF_SPEED): _schema(UNIT_REVOLUTIONS_PER_MINUTE, 0),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_GRUNDFOS_ALPHA_ID])
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_FLOW, hub.set_flow_sensor)
    await sensors(CONF_HEAD, hub.set_head_sensor)
    await sensors(CONF_MEDIUM_TEMPERATURE, hub.set_medium_temperature_sensor)
    await sensors(CONF_VOLTAGE, hub.set_voltage_sensor)
    await sensors(CONF_CURRENT, hub.set_current_sensor)
    await sensors(CONF_POWER, hub.set_power_sensor)
    await sensors(CONF_SPEED, hub.set_speed_sensor)
