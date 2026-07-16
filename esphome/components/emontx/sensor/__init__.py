import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_POWER_FACTOR,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_EMPTY,
    UNIT_PULSES,
    UNIT_VOLT,
    UNIT_WATT,
    UNIT_WATT_HOURS,
)
from esphome.types import ConfigType

from .. import CONF_EMONTX_ID, CONF_TAG_NAME, EmonTx, emontx_ns

EmonTxSensor = emontx_ns.class_("EmonTxSensor", sensor.Sensor, cg.Component)

_COMMON = {
    cv.GenerateID(CONF_EMONTX_ID): cv.use_id(EmonTx),
    cv.Required(CONF_TAG_NAME): cv.string,
}

# One fully-validated schema per tag prefix (E1, P1, V1, I1, T1, ...)
_SCHEMA_BY_PREFIX = {
    "P": sensor.sensor_schema(
        EmonTxSensor,
        unit_of_measurement=UNIT_WATT,
        device_class=DEVICE_CLASS_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
        accuracy_decimals=0,
    ).extend(_COMMON),
    "E": sensor.sensor_schema(
        EmonTxSensor,
        unit_of_measurement=UNIT_WATT_HOURS,
        device_class=DEVICE_CLASS_ENERGY,
        state_class=STATE_CLASS_TOTAL_INCREASING,
        accuracy_decimals=0,
    ).extend(_COMMON),
    "V": sensor.sensor_schema(
        EmonTxSensor,
        unit_of_measurement=UNIT_VOLT,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
        accuracy_decimals=2,
    ).extend(_COMMON),
    "I": sensor.sensor_schema(
        EmonTxSensor,
        unit_of_measurement=UNIT_AMPERE,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
        accuracy_decimals=2,
    ).extend(_COMMON),
    "T": sensor.sensor_schema(
        EmonTxSensor,
        unit_of_measurement=UNIT_CELSIUS,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
        accuracy_decimals=2,
    ).extend(_COMMON),
}

# One fully-validated schema per tag pattern (PULSE*, PF*)
_SCHEMA_BY_PATTERN = {
    "PULSE": sensor.sensor_schema(
        EmonTxSensor,
        unit_of_measurement=UNIT_PULSES,
        device_class=DEVICE_CLASS_ENERGY,
        state_class=STATE_CLASS_TOTAL_INCREASING,
        accuracy_decimals=0,
    ).extend(_COMMON),
    "PF": sensor.sensor_schema(
        EmonTxSensor,
        unit_of_measurement=UNIT_EMPTY,
        device_class=DEVICE_CLASS_POWER_FACTOR,
        state_class=STATE_CLASS_MEASUREMENT,
        accuracy_decimals=2,
    ).extend(_COMMON),
}

# Fallback for tags that match no known prefix or pattern
_SCHEMA_GENERIC = sensor.sensor_schema(
    EmonTxSensor,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=0,
).extend(_COMMON)

# Minimal first-pass schema: only needs to validate tag_name so the dispatcher
# can select the correct full schema.
_TAG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_EMONTX_ID): cv.use_id(EmonTx),
        cv.Required(CONF_TAG_NAME): cv.string,
    },
    extra=cv.ALLOW_EXTRA,
)


def _emontx_sensor_schema(config: ConfigType) -> ConfigType:
    """Select and apply the correct sensor schema based on the tag name."""
    tag = config[CONF_TAG_NAME].upper()

    for pattern, schema in _SCHEMA_BY_PATTERN.items():
        if tag.startswith(pattern):
            return schema(config)

    if len(tag) >= 2 and tag[1:].isdigit() and (schema := _SCHEMA_BY_PREFIX.get(tag[0])):
        return schema(config)

    return _SCHEMA_GENERIC(config)


CONFIG_SCHEMA = cv.All(_TAG_SCHEMA, _emontx_sensor_schema)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)
    hub = await cg.get_variable(config[CONF_EMONTX_ID])
    cg.add(hub.register_sensor(config[CONF_TAG_NAME], var))
