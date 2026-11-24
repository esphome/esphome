import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.const import CONF_SOURCES
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACCURACY_DECIMALS,
    CONF_DEVICE_CLASS,
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_ID,
    CONF_RANGE,
    CONF_SOURCE,
    CONF_SUM,
    CONF_TYPE,
    CONF_UNIT_OF_MEASUREMENT,
    CONF_WEIGHT,
)
from esphome.core.entity_helpers import inherit_property_from
from esphome.types import ConfigType

CODEOWNERS = ["@kahrendt"]

combination_ns = cg.esphome_ns.namespace("combination")

KalmanCombinationComponent = combination_ns.class_(
    "KalmanCombinationComponent", cg.Component, sensor.Sensor
)
LinearCombinationComponent = combination_ns.class_(
    "LinearCombinationComponent", cg.Component, sensor.Sensor
)
WeightedMeanCombinationComponent = combination_ns.class_(
    "WeightedMeanCombinationComponent", cg.Component, sensor.Sensor
)
MaximumCombinationComponent = combination_ns.class_(
    "MaximumCombinationComponent", cg.Component, sensor.Sensor
)
MeanCombinationComponent = combination_ns.class_(
    "MeanCombinationComponent", cg.Component, sensor.Sensor
)
MedianCombinationComponent = combination_ns.class_(
    "MedianCombinationComponent", cg.Component, sensor.Sensor
)
MinimumCombinationComponent = combination_ns.class_(
    "MinimumCombinationComponent", cg.Component, sensor.Sensor
)
MostRecentCombinationComponent = combination_ns.class_(
    "MostRecentCombinationComponent", cg.Component, sensor.Sensor
)
RangeCombinationComponent = combination_ns.class_(
    "RangeCombinationComponent", cg.Component, sensor.Sensor
)
SumCombinationComponent = combination_ns.class_(
    "SumCombinationComponent", cg.Component, sensor.Sensor
)

CONF_COEFFICIENT = "coefficient"
CONF_COEFFECIENT = "coeffecient"  # Removed in 2026.12.0
CONF_ERROR = "error"
CONF_KALMAN = "kalman"
CONF_LINEAR = "linear"
CONF_MAX = "max"
CONF_MEAN = "mean"
CONF_MEDIAN = "median"
CONF_MIN = "min"
CONF_MOST_RECENTLY_UPDATED = "most_recently_updated"
CONF_PROCESS_STD_DEV = "process_std_dev"
CONF_STD_DEV = "std_dev"
CONF_WEIGHTED_MEAN = "weighted_mean"

ONE_PARAMETER_SOURCE_KEYS = {
    CONF_KALMAN: CONF_ERROR,
    CONF_LINEAR: CONF_COEFFICIENT,
    CONF_WEIGHTED_MEAN: CONF_WEIGHT,
}


KALMAN_SOURCE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_SOURCE): cv.use_id(sensor.Sensor),
        cv.Required(CONF_ERROR): cv.templatable(cv.positive_float),
    }
)


LINEAR_SOURCE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_SOURCE): cv.use_id(sensor.Sensor),
        cv.Required(CONF_COEFFICIENT): cv.templatable(cv.float_),
        # Removed in 2026.12.0 - kept to provide helpful error message
        cv.Optional(CONF_COEFFECIENT): cv.invalid(
            "The misspelled 'coeffecient' option was removed in ESPHome 2026.12.0, "
            "use 'coefficient' instead."
        ),
    }
)

WEIGHTED_MEAN_SOURCE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_SOURCE): cv.use_id(sensor.Sensor),
        cv.Required(CONF_WEIGHT): cv.templatable(cv.positive_float),
    }
)

SENSOR_ONLY_SOURCE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_SOURCE): cv.use_id(sensor.Sensor),
    }
)


def _sources_schema(source_schema: cv.Schema) -> cv.All:
    return cv.All(cv.ensure_list(source_schema), cv.Length(min=1))


CONFIG_SCHEMA = cv.typed_schema(
    {
        CONF_KALMAN: sensor.sensor_schema(KalmanCombinationComponent)
        .extend(cv.COMPONENT_SCHEMA)
        .extend(
            {
                cv.Required(CONF_PROCESS_STD_DEV): cv.positive_float,
                cv.Required(CONF_SOURCES): _sources_schema(KALMAN_SOURCE_SCHEMA),
                cv.Optional(CONF_STD_DEV): sensor.sensor_schema(),
            }
        ),
        CONF_LINEAR: sensor.sensor_schema(LinearCombinationComponent)
        .extend(cv.COMPONENT_SCHEMA)
        .extend({cv.Required(CONF_SOURCES): _sources_schema(LINEAR_SOURCE_SCHEMA)}),
        CONF_MAX: sensor.sensor_schema(MaximumCombinationComponent)
        .extend(cv.COMPONENT_SCHEMA)
        .extend(
            {cv.Required(CONF_SOURCES): _sources_schema(SENSOR_ONLY_SOURCE_SCHEMA)}
        ),
        CONF_MEAN: sensor.sensor_schema(MeanCombinationComponent)
        .extend(cv.COMPONENT_SCHEMA)
        .extend(
            {cv.Required(CONF_SOURCES): _sources_schema(SENSOR_ONLY_SOURCE_SCHEMA)}
        ),
        CONF_MEDIAN: sensor.sensor_schema(MedianCombinationComponent)
        .extend(cv.COMPONENT_SCHEMA)
        .extend(
            {cv.Required(CONF_SOURCES): _sources_schema(SENSOR_ONLY_SOURCE_SCHEMA)}
        ),
        CONF_MIN: sensor.sensor_schema(MinimumCombinationComponent)
        .extend(cv.COMPONENT_SCHEMA)
        .extend(
            {cv.Required(CONF_SOURCES): _sources_schema(SENSOR_ONLY_SOURCE_SCHEMA)}
        ),
        CONF_MOST_RECENTLY_UPDATED: sensor.sensor_schema(MostRecentCombinationComponent)
        .extend(cv.COMPONENT_SCHEMA)
        .extend(
            {cv.Required(CONF_SOURCES): _sources_schema(SENSOR_ONLY_SOURCE_SCHEMA)}
        ),
        CONF_RANGE: sensor.sensor_schema(RangeCombinationComponent)
        .extend(cv.COMPONENT_SCHEMA)
        .extend(
            {cv.Required(CONF_SOURCES): _sources_schema(SENSOR_ONLY_SOURCE_SCHEMA)}
        ),
        CONF_SUM: sensor.sensor_schema(SumCombinationComponent)
        .extend(cv.COMPONENT_SCHEMA)
        .extend(
            {cv.Required(CONF_SOURCES): _sources_schema(SENSOR_ONLY_SOURCE_SCHEMA)}
        ),
        CONF_WEIGHTED_MEAN: sensor.sensor_schema(WeightedMeanCombinationComponent)
        .extend(cv.COMPONENT_SCHEMA)
        .extend(
            {cv.Required(CONF_SOURCES): _sources_schema(WEIGHTED_MEAN_SOURCE_SCHEMA)}
        ),
    }
)


# Inherit some sensor values from the first source, for both the state and the error value
# CONF_STATE_CLASS could also be inherited, but might lead to unexpected behaviour with "total_increasing"
properties_to_inherit = [
    CONF_ACCURACY_DECIMALS,
    CONF_DEVICE_CLASS,
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_UNIT_OF_MEASUREMENT,
]
inherit_schema_for_state = [
    inherit_property_from(property, [CONF_SOURCES, 0, CONF_SOURCE])
    for property in properties_to_inherit
]
inherit_schema_for_std_dev = [
    inherit_property_from([CONF_STD_DEV, property], [CONF_SOURCES, 0, CONF_SOURCE])
    for property in properties_to_inherit
]

FINAL_VALIDATE_SCHEMA = cv.All(
    *inherit_schema_for_state,
    *inherit_schema_for_std_dev,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    if process_std_dev := config.get(CONF_PROCESS_STD_DEV):
        cg.add(var.set_process_std_dev(process_std_dev))

    if config[CONF_TYPE] in ONE_PARAMETER_SOURCE_KEYS:
        storage = cg.static_storage(
            config[CONF_ID],
            config[CONF_ID].type.class_("SensorSource"),
            len(config[CONF_SOURCES]),
        )
        cg.add(var.set_source_storage(storage))
    else:
        storage = cg.static_storage(
            config[CONF_ID], sensor.Sensor.operator("ptr"), len(config[CONF_SOURCES])
        )
        cg.add(var.set_sensor_storage(storage))

    for source_conf in config[CONF_SOURCES]:
        source = await cg.get_variable(source_conf[CONF_SOURCE])
        if (
            parameter_key := ONE_PARAMETER_SOURCE_KEYS.get(config[CONF_TYPE])
        ) is not None:
            parameter = await cg.templatable(
                source_conf[parameter_key],
                [(float, "x")],
                cg.float_,
            )
            cg.add(var.add_source(source, parameter))
        else:
            cg.add(var.add_source(source))

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_STD_DEV, var.set_std_dev_sensor)
