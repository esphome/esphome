from esphome import automation
import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACCURACY_DECIMALS,
    CONF_ALPHA,
    CONF_DEVICE_CLASS,
    CONF_ICON,
    CONF_ID,
    CONF_RESTORE,
    CONF_SENSOR,
    CONF_STATE_CLASS,
    CONF_TIME_CONSTANT,
    CONF_UNIT_OF_MEASUREMENT,
)
from esphome.core.entity_helpers import inherit_property_from
from esphome.types import ConfigType

exponential_moving_average_ns = cg.esphome_ns.namespace("exponential_moving_average")
ExponentialMovingAverageSensor = exponential_moving_average_ns.class_(
    "ExponentialMovingAverageSensor", sensor.Sensor, cg.Component
)

TimeWeighting = exponential_moving_average_ns.enum("TimeWeighting")
TIME_WEIGHTINGS: dict[str, cg.MockObj] = {
    "new": TimeWeighting.TIME_WEIGHTING_NEW,
    "previous": TimeWeighting.TIME_WEIGHTING_PREVIOUS,
    "linear": TimeWeighting.TIME_WEIGHTING_LINEAR,
}

CONF_TIME_WEIGHTING: str = "time_weighting"

DEFAULT_ALPHA: float = 0.1


def inherit_accuracy_decimals(decimals: int, config: ConfigType) -> int:
    # An average carries more precision than the individual readings.
    return decimals + 1


def validate_time_weighting(config: ConfigType) -> ConfigType:
    if CONF_TIME_WEIGHTING in config and CONF_TIME_CONSTANT not in config:
        raise cv.Invalid(
            f"'{CONF_TIME_WEIGHTING}' can only be used with '{CONF_TIME_CONSTANT}'",
            path=[CONF_TIME_WEIGHTING],
        )
    return config


CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(ExponentialMovingAverageSensor)
    .extend(
        {
            cv.Required(CONF_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_ALPHA): cv.All(
                cv.float_, cv.Range(min=0, min_included=False, max=1)
            ),
            cv.Optional(CONF_TIME_CONSTANT): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_TIME_WEIGHTING): cv.enum(TIME_WEIGHTINGS, lower=True),
            cv.Optional(CONF_RESTORE, default=True): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    cv.has_at_most_one_key(CONF_ALPHA, CONF_TIME_CONSTANT),
    validate_time_weighting,
)

FINAL_VALIDATE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(ExponentialMovingAverageSensor),
            cv.Optional(CONF_ICON): cv.icon,
            cv.Optional(CONF_UNIT_OF_MEASUREMENT): sensor.validate_unit_of_measurement,
            cv.Optional(CONF_ACCURACY_DECIMALS): sensor.validate_accuracy_decimals,
            cv.Optional(CONF_DEVICE_CLASS): sensor.validate_device_class,
            cv.Optional(CONF_STATE_CLASS): sensor.validate_state_class,
            cv.Required(CONF_SENSOR): cv.use_id(sensor.Sensor),
        },
        extra=cv.ALLOW_EXTRA,
    ),
    inherit_property_from(CONF_ICON, CONF_SENSOR),
    inherit_property_from(CONF_UNIT_OF_MEASUREMENT, CONF_SENSOR),
    inherit_property_from(
        CONF_ACCURACY_DECIMALS, CONF_SENSOR, transform=inherit_accuracy_decimals
    ),
    inherit_property_from(CONF_DEVICE_CLASS, CONF_SENSOR),
    inherit_property_from(CONF_STATE_CLASS, CONF_SENSOR),
)


async def to_code(config: ConfigType) -> None:
    source = await cg.get_variable(config[CONF_SENSOR])
    var = cg.new_Pvariable(config[CONF_ID], source)
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    if (time_constant := config.get(CONF_TIME_CONSTANT)) is not None:
        cg.add(var.set_time_constant(time_constant))
        if (weighting := config.get(CONF_TIME_WEIGHTING)) is not None:
            cg.add(var.set_time_weighting(weighting))
    else:
        cg.add(var.set_alpha(config.get(CONF_ALPHA, DEFAULT_ALPHA)))
    cg.add(var.set_restore(config[CONF_RESTORE]))


automation.register_apply_action(
    "sensor.exponential_moving_average.reset",
    automation.maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(ExponentialMovingAverageSensor),
        }
    ),
    automation.ApplyCall("reset()"),
)
