import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACCURACY_DECIMALS,
    CONF_ICON,
    CONF_ID,
    CONF_SOURCE_ID,
    CONF_UNIT_OF_MEASUREMENT,
)
from esphome.core.entity_helpers import inherit_property_from
from esphome.types import ConfigType

derivative_ns = cg.esphome_ns.namespace("derivative")
DerivativeSensor = derivative_ns.class_("DerivativeSensor", sensor.Sensor, cg.Component)

DerivativeSensorTime = derivative_ns.enum("DerivativeSensorTime")
DERIVATIVE_TIMES = {
    "ms": DerivativeSensorTime.DERIVATIVE_SENSOR_TIME_MILLISECOND,
    "s": DerivativeSensorTime.DERIVATIVE_SENSOR_TIME_SECOND,
    "min": DerivativeSensorTime.DERIVATIVE_SENSOR_TIME_MINUTE,
    "h": DerivativeSensorTime.DERIVATIVE_SENSOR_TIME_HOUR,
    "d": DerivativeSensorTime.DERIVATIVE_SENSOR_TIME_DAY,
}

CONF_TIME_UNIT = "time_unit"


def inherit_unit_of_measurement(uom: str, config: ConfigType) -> str:
    # The derivative is a rate: the source's unit per the configured time unit
    # (e.g. a distance in m with time_unit "s" becomes a speed in m/s).
    return f"{uom}/{config[CONF_TIME_UNIT]}"


def inherit_accuracy_decimals(decimals: int, config: ConfigType) -> int:
    return decimals + 2


CONFIG_SCHEMA = (
    sensor.sensor_schema(DerivativeSensor)
    .extend(
        {
            cv.Required(CONF_SOURCE_ID): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_TIME_UNIT, default="s"): cv.enum(
                DERIVATIVE_TIMES, lower=True
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


FINAL_VALIDATE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(DerivativeSensor),
            cv.Optional(CONF_ICON): cv.icon,
            cv.Optional(CONF_UNIT_OF_MEASUREMENT): sensor.validate_unit_of_measurement,
            cv.Optional(CONF_ACCURACY_DECIMALS): sensor.validate_accuracy_decimals,
            cv.Required(CONF_SOURCE_ID): cv.use_id(sensor.Sensor),
        },
        extra=cv.ALLOW_EXTRA,
    ),
    inherit_property_from(CONF_ICON, CONF_SOURCE_ID),
    inherit_property_from(
        CONF_UNIT_OF_MEASUREMENT, CONF_SOURCE_ID, transform=inherit_unit_of_measurement
    ),
    inherit_property_from(
        CONF_ACCURACY_DECIMALS, CONF_SOURCE_ID, transform=inherit_accuracy_decimals
    ),
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    await cg.add_variable(var.set_source_sensor, config[CONF_SOURCE_ID])
    cg.add(var.set_time(config[CONF_TIME_UNIT]))
