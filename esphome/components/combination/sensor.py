import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
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
)
from esphome.core.entity_helpers import inherit_property_from

CODEOWNERS = ["@Cat-Ion", "@kahrendt"]

combination_ns = cg.esphome_ns.namespace("combination")

KalmanCombinationComponent = combination_ns.class_(
    "KalmanCombinationComponent", cg.Component, sensor.Sensor
)
LinearCombinationComponent = combination_ns.class_(
    "LinearCombinationComponent", cg.Component, sensor.Sensor
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

CONF_COEFFECIENT = "coeffecient"
CONF_ERROR = "error"
CONF_KALMAN = "kalman"
CONF_LINEAR = "linear"
CONF_MAX = "max"
CONF_MEAN = "mean"
CONF_MEDIAN = "median"
CONF_MIN = "min"
CONF_MOST_RECENTLY_UPDATED = "most_recently_updated"
CONF_PROCESS_STD_DEV = "process_std_dev"
CONF_SOURCES = "sources"
CONF_STD_DEV = "std_dev"


KALMAN_SOURCE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_SOURCE): cv.use_id(sensor.Sensor),
        cv.Required(CONF_ERROR): cv.templatable(cv.positive_float),
    }
)

LINEAR_SOURCE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_SOURCE): cv.use_id(sensor.Sensor),
        cv.Required(CONF_COEFFECIENT): cv.templatable(cv.float_),
    }
)

SENSOR_ONLY_SOURCE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_SOURCE): cv.use_id(sensor.Sensor),
    }
)

CONFIG_SCHEMA = cv.typed_schema(
    {
        CONF_KALMAN: sensor.sensor_schema(KalmanCombinationComponent)
        .extend(cv.COMPONENT_SCHEMA)
        .extend(
            {
                cv.Required(CONF_PROCESS_STD_DEV): cv.positive_float,
                cv.Required(CONF_SOURCES): cv.ensure_list(KALMAN_SOURCE_SCHEMA),
                cv.Optional(CONF_STD_DEV): sensor.sensor_schema(),
            }
        ),
        CONF_LINEAR: sensor.sensor_schema(LinearCombinationComponent)
        .extend(cv.COMPONENT_SCHEMA)
        .extend({cv.Required(CONF_SOURCES): cv.ensure_list(LINEAR_SOURCE_SCHEMA)}),
        CONF_MAX: sensor.sensor_schema(MaximumCombinationComponent)
        .extend(cv.COMPONENT_SCHEMA)
        .extend({cv.Required(CONF_SOURCES): cv.ensure_list(SENSOR_ONLY_SOURCE_SCHEMA)}),
        CONF_MEAN: sensor.sensor_schema(MeanCombinationComponent)
        .extend(cv.COMPONENT_SCHEMA)
        .extend({cv.Required(CONF_SOURCES): cv.ensure_list(SENSOR_ONLY_SOURCE_SCHEMA)}),
        CONF_MEDIAN: sensor.sensor_schema(MedianCombinationComponent)
        .extend(cv.COMPONENT_SCHEMA)
        .extend({cv.Required(CONF_SOURCES): cv.ensure_list(SENSOR_ONLY_SOURCE_SCHEMA)}),
        CONF_MIN: sensor.sensor_schema(MinimumCombinationComponent)
        .extend(cv.COMPONENT_SCHEMA)
        .extend({cv.Required(CONF_SOURCES): cv.ensure_list(SENSOR_ONLY_SOURCE_SCHEMA)}),
        CONF_MOST_RECENTLY_UPDATED: sensor.sensor_schema(MostRecentCombinationComponent)
        .extend(cv.COMPONENT_SCHEMA)
        .extend({cv.Required(CONF_SOURCES): cv.ensure_list(SENSOR_ONLY_SOURCE_SCHEMA)}),
        CONF_RANGE: sensor.sensor_schema(RangeCombinationComponent)
        .extend(cv.COMPONENT_SCHEMA)
        .extend({cv.Required(CONF_SOURCES): cv.ensure_list(SENSOR_ONLY_SOURCE_SCHEMA)}),
        CONF_SUM: sensor.sensor_schema(SumCombinationComponent)
        .extend(cv.COMPONENT_SCHEMA)
        .extend({cv.Required(CONF_SOURCES): cv.ensure_list(SENSOR_ONLY_SOURCE_SCHEMA)}),
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


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    if proces_std_dev := config.get(CONF_PROCESS_STD_DEV):
        cg.add(var.set_process_std_dev(proces_std_dev))

    for source_conf in config[CONF_SOURCES]:
        source = await cg.get_variable(source_conf[CONF_SOURCE])
        if config[CONF_TYPE] == CONF_KALMAN:
            error = await cg.templatable(
                source_conf[CONF_ERROR],
                [(float, "x")],
                cg.float_,
            )
            cg.add(var.add_source(source, error))
        elif config[CONF_TYPE] == CONF_LINEAR:
            coeffecient = await cg.templatable(
                source_conf[CONF_COEFFECIENT],
                [(float, "x")],
                cg.float_,
            )
            cg.add(var.add_source(source, coeffecient))
        else:
            cg.add(var.add_source(source))

    if CONF_STD_DEV in config:
        sens = await sensor.new_sensor(config[CONF_STD_DEV])
        cg.add(var.set_std_dev_sensor(sens))
