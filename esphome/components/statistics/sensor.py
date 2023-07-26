import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import sensor
from esphome.const import (
    CONF_ACCURACY_DECIMALS,
    CONF_COUNT,
    CONF_DEVICE_CLASS,
    CONF_DURATION,
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_ID,
    CONF_RESTORE,
    CONF_SEND_EVERY,
    CONF_SEND_FIRST_AT,
    CONF_SOURCE_ID,
    CONF_TYPE,
    CONF_UNIT_OF_MEASUREMENT,
    CONF_WINDOW_SIZE,
    DEVICE_CLASS_DURATION,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL,
    UNIT_MILLISECOND,
    UNIT_SECOND,
)
from esphome.core.entity_helpers import inherit_property_from

CODEOWNERS = ["@kahrendt"]
DEPENDENCIES = ["time"]

statistics_ns = cg.esphome_ns.namespace("statistics")
StatisticsComponent = statistics_ns.class_("StatisticsComponent", cg.Component)

######################
# Automation Actions #
######################

# Reset action that clears all queued aggragates
ResetAction = statistics_ns.class_("ResetAction", automation.Action)

# Force all sensors to publish
ForcePublishAction = statistics_ns.class_("ForcePublishAction", automation.Action)

#####################
# Definable sensors #
#####################

CONF_MAX = "max"
CONF_MEAN = "mean"
CONF_MIN = "min"
CONF_SINCE_ARGMAX = "since_argmax"
CONF_SINCE_ARGMIN = "since_argmin"
CONF_STD_DEV = "std_dev"
CONF_TREND = "trend"

################
# Window Types #
################

CONF_SLIDING = "sliding"
CONF_CONTINUOUS = "continuous"
CONF_CONTINUOUS_LONG_TERM = "continuous_long_term"
CONF_WINDOW = "window"

WindowType = statistics_ns.enum("WindowType")
WINDOW_TYPES = {
    CONF_SLIDING: WindowType.WINDOW_TYPE_SLIDING,
    CONF_CONTINUOUS: WindowType.WINDOW_TYPE_CONTINUOUS,
    CONF_CONTINUOUS_LONG_TERM: WindowType.WINDOW_TYPE_CONTINUOUS_LONG_TERM,
}

################################################
# Configuration Options for Chunks and Windows #
################################################

CONF_CHUNK_SIZE = "chunk_size"
CONF_CHUNK_DURATION = "chunk_duration"

CONF_WINDOW_DURATION = "window_duration"

##########################
# Measurement Group Type #
##########################

CONF_GROUP_TYPE = "group_type"

GroupType = statistics_ns.enum("GroupType")
GROUP_TYPES = {
    "sample": GroupType.SAMPLE_GROUP_TYPE,
    "population": GroupType.POPULATION_GROUP_TYPE,
}


#################
# Average Types #
#################

CONF_AVERAGE_TYPE = "average_type"

AverageType = statistics_ns.enum("AverageType")
AVERAGE_TYPES = {
    "simple": AverageType.SIMPLE_AVERAGE,
    "time_weighted": AverageType.TIME_WEIGHTED_AVERAGE,
}

########################
# Time Units for Trend #
########################

CONF_TIME_UNIT = "time_unit"

TimeConversionFactor = statistics_ns.enum("TimeConversionFactor")
TIME_CONVERSION_FACTORS = {
    "ms": TimeConversionFactor.FACTOR_MS,
    "s": TimeConversionFactor.FACTOR_S,
    "min": TimeConversionFactor.FACTOR_MIN,
    "h": TimeConversionFactor.FACTOR_HOUR,
    "d": TimeConversionFactor.FACTOR_DAY,
}

#########################################
# Sensor Lists for Property Inheritance #
#########################################

SENSOR_LIST_WITH_ORIGINAL_UNITS = [
    CONF_MAX,
    CONF_MEAN,
    CONF_MIN,
    CONF_STD_DEV,
]

SENSOR_LIST_WITH_MODIFIED_UNITS = [
    CONF_COUNT,
    CONF_DURATION,
    CONF_SINCE_ARGMAX,
    CONF_SINCE_ARGMIN,
    CONF_TREND,
]

SENSOR_LIST_WITH_SAME_ACCURACY_DECIMALS = [
    CONF_MAX,
    CONF_MEAN,
    CONF_MIN,
]

SENSOR_LIST_WITH_INCREASED_ACCURACY_DECIMALS = [
    CONF_STD_DEV,
    CONF_TREND,
]

#####################################################
# Transformation Functions for Inherited Properties #
#####################################################


# Trend's unit is in original unit of measurement divides by time unit of measurement
def transform_trend_unit_of_measurement(uom, config):
    denominator = config[CONF_TIME_UNIT]
    return uom + "/" + denominator


# Increases accuracy decimals by 2
# Borrowed from sensor/integration/sensor.py (accessed July 2023)
def transform_accuracy_decimals(decimals, config):
    return decimals + 2


#####################################
# Confiuration Validation Functions #
#####################################


# Borrowed from sensor/__init__.py (accessed July 2023)
def validate_send_first_at(config):
    send_first_at = config.get(CONF_SEND_FIRST_AT)
    send_every = config[CONF_SEND_EVERY]
    if send_every > 0:  # If send_every == 0, then automatic publication is disabled
        if send_first_at is not None and send_first_at > send_every:
            raise cv.Invalid(
                f"send_first_at must be smaller than or equal to send_every! {send_first_at} <= {send_every}"
            )
    return config


# Ensures that the trend sensor is not enabled if restore from flash is enabled
def validate_no_trend_and_restore(config):
    window_config = config.get(CONF_WINDOW)

    if (CONF_RESTORE in window_config) and (CONF_TREND in config):
        raise cv.Invalid("The trend sensor cannot be configured if restore is enabled")
    return config


###################
# Inherit Schemas #
###################

PROPERTIES_TO_INHERIT_WITH_ORIGINAL_UNIT_SENSORS = [
    CONF_DEVICE_CLASS,
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_UNIT_OF_MEASUREMENT,
]

PROPERTIES_TO_INHERIT_WITH_MODIFIED_UNIT_SENSORS = [
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
]

inherit_schema_for_same_unit_sensors = [
    inherit_property_from([sensor_config, property], CONF_SOURCE_ID)
    for property in PROPERTIES_TO_INHERIT_WITH_ORIGINAL_UNIT_SENSORS
    for sensor_config in SENSOR_LIST_WITH_ORIGINAL_UNITS
]
inherit_schema_for_new_unit_sensors = [
    inherit_property_from([sensor_config, property], CONF_SOURCE_ID)
    for property in PROPERTIES_TO_INHERIT_WITH_MODIFIED_UNIT_SENSORS
    for sensor_config in SENSOR_LIST_WITH_MODIFIED_UNITS
]

inherit_accuracy_decimals_without_transformation = [
    inherit_property_from([sensor_config, CONF_ACCURACY_DECIMALS], CONF_SOURCE_ID)
    for sensor_config in SENSOR_LIST_WITH_SAME_ACCURACY_DECIMALS
]

inherit_accuracy_decimals_with_transformation = [
    inherit_property_from(
        [sensor_config, CONF_ACCURACY_DECIMALS],
        CONF_SOURCE_ID,
        transform=transform_accuracy_decimals,
    )
    for sensor_config in SENSOR_LIST_WITH_INCREASED_ACCURACY_DECIMALS
]

#########################
# Configuration Schemas #
#########################


SLIDING_WINDOW_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_WINDOW_SIZE): cv.positive_not_null_int,
            cv.Optional(CONF_CHUNK_SIZE): cv.positive_not_null_int,
            cv.Optional(CONF_CHUNK_DURATION): cv.time_period,
            cv.Optional(CONF_SEND_EVERY, default=1): cv.positive_not_null_int,
            cv.Optional(CONF_SEND_FIRST_AT, default=1): cv.positive_not_null_int,
        },
    ),
    validate_send_first_at,
)

CONTINUOUS_WINDOW_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_WINDOW_SIZE): cv.positive_int,  # 0 disables window reset
            cv.Optional(CONF_WINDOW_DURATION): cv.time_period,
            cv.Optional(CONF_CHUNK_SIZE): cv.positive_not_null_int,
            cv.Optional(CONF_CHUNK_DURATION): cv.time_period,
            cv.Optional(
                CONF_SEND_EVERY, default=1
            ): cv.positive_int,  # 0 disables automatic updates
            cv.Optional(CONF_SEND_FIRST_AT, default=1): cv.positive_not_null_int,
            cv.Optional(CONF_RESTORE): cv.boolean,
        },
    ),
    validate_send_first_at,
    cv.has_at_least_one_key(CONF_WINDOW_SIZE, CONF_WINDOW_DURATION),
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(StatisticsComponent),
            cv.Required(CONF_SOURCE_ID): cv.use_id(sensor.Sensor),
            cv.Required(CONF_WINDOW): cv.typed_schema(
                {
                    CONF_SLIDING: SLIDING_WINDOW_SCHEMA,
                    CONF_CONTINUOUS: CONTINUOUS_WINDOW_SCHEMA,
                    CONF_CONTINUOUS_LONG_TERM: CONTINUOUS_WINDOW_SCHEMA,
                }
            ),
            cv.Optional(CONF_AVERAGE_TYPE, default="simple"): cv.enum(
                AVERAGE_TYPES, lower=True
            ),
            cv.Optional(CONF_GROUP_TYPE, default="sample"): cv.enum(
                GROUP_TYPES, lower=True
            ),
            cv.Optional(CONF_TIME_UNIT, default="s"): cv.enum(
                TIME_CONVERSION_FACTORS, lower=True
            ),
            cv.Optional(CONF_SINCE_ARGMAX): sensor.sensor_schema(
                state_class=STATE_CLASS_MEASUREMENT,
                device_class=DEVICE_CLASS_DURATION,
                unit_of_measurement=UNIT_SECOND,
            ),
            cv.Optional(CONF_SINCE_ARGMIN): sensor.sensor_schema(
                state_class=STATE_CLASS_MEASUREMENT,
                device_class=DEVICE_CLASS_DURATION,
                unit_of_measurement=UNIT_SECOND,
            ),
            cv.Optional(CONF_COUNT): sensor.sensor_schema(
                state_class=STATE_CLASS_TOTAL,
            ),
            cv.Optional(CONF_DURATION): sensor.sensor_schema(
                state_class=STATE_CLASS_MEASUREMENT,
                device_class=DEVICE_CLASS_DURATION,
                unit_of_measurement=UNIT_MILLISECOND,
            ),
            cv.Optional(CONF_MAX): sensor.sensor_schema(
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_MEAN): sensor.sensor_schema(
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_MIN): sensor.sensor_schema(
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_STD_DEV): sensor.sensor_schema(
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TREND): sensor.sensor_schema(
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        },
    ).extend(cv.COMPONENT_SCHEMA),
    validate_no_trend_and_restore,
)


# Handles inheriting properties from the source sensor
FINAL_VALIDATE_SCHEMA = cv.All(
    *inherit_schema_for_new_unit_sensors,
    *inherit_schema_for_same_unit_sensors,
    *inherit_accuracy_decimals_without_transformation,
    *inherit_accuracy_decimals_with_transformation,
    inherit_property_from(
        [CONF_TREND, CONF_UNIT_OF_MEASUREMENT],
        CONF_SOURCE_ID,
        transform=transform_trend_unit_of_measurement,
    ),
)

###################
# Code Generation #
###################


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    source = await cg.get_variable(config[CONF_SOURCE_ID])

    cg.add(var.set_hash(str(config[CONF_ID])))

    cg.add(var.set_source_sensor(source))

    cg.add(var.set_average_type(config[CONF_AVERAGE_TYPE]))
    cg.add(var.set_group_type(config[CONF_GROUP_TYPE]))
    cg.add(var.set_time_conversion_factor(config[CONF_TIME_UNIT]))

    ####################
    # Configure Window #
    ####################
    window_config = config["window"]
    constant = WINDOW_TYPES[window_config[CONF_TYPE]]
    cg.add(var.set_window_type(constant))

    # Setup window size
    if CONF_WINDOW_SIZE in window_config:
        if window_config[CONF_WINDOW_SIZE] > 0:
            cg.add(var.set_window_size(window_config[CONF_WINDOW_SIZE]))

    if CONF_WINDOW_DURATION in window_config:
        cg.add(
            var.set_window_duration(
                window_config[CONF_WINDOW_DURATION].total_milliseconds
            )
        )

    # Setup chunk size
    if (CONF_CHUNK_SIZE not in window_config) and (
        CONF_CHUNK_DURATION not in window_config
    ):
        cg.add(var.set_chunk_size(1))
    if CONF_CHUNK_SIZE in window_config:
        if window_config[CONF_CHUNK_SIZE] > 0:
            cg.add(var.set_chunk_size(window_config[CONF_CHUNK_SIZE]))
    if CONF_CHUNK_DURATION in window_config:
        cg.add(
            var.set_chunk_duration(
                window_config[CONF_CHUNK_DURATION].total_milliseconds
            )
        )

    # Setup send parameters
    cg.add(var.set_send_every(window_config[CONF_SEND_EVERY]))
    cg.add(var.set_first_at(window_config[CONF_SEND_FIRST_AT]))

    # Setup restore setting
    if CONF_RESTORE in window_config:
        cg.add(var.set_restore(window_config[CONF_RESTORE]))

    ############################
    # Setup Configured Sensors #
    ############################
    if CONF_COUNT in config:
        conf = config[CONF_COUNT]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_count_sensor(sens))

    if CONF_DURATION in config:
        conf = config[CONF_DURATION]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_duration_sensor(sens))

    if CONF_MAX in config:
        conf = config[CONF_MAX]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_max_sensor(sens))

    if CONF_MEAN in config:
        conf = config[CONF_MEAN]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_mean_sensor(sens))

    if CONF_MIN in config:
        conf = config[CONF_MIN]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_min_sensor(sens))

    if CONF_SINCE_ARGMAX in config:
        conf = config[CONF_SINCE_ARGMAX]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_since_argmax_sensor(sens))

    if CONF_SINCE_ARGMIN in config:
        conf = config[CONF_SINCE_ARGMIN]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_since_argmin_sensor(sens))

    if CONF_STD_DEV in config:
        conf = config[CONF_STD_DEV]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_std_dev_sensor(sens))

    if CONF_TREND in config:
        conf = config[CONF_TREND]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_trend_sensor(sens))


######################
# Automation Actions #
######################


@automation.register_action(
    "sensor.statistics.reset",
    ResetAction,
    automation.maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(StatisticsComponent),
        }
    ),
)
@automation.register_action(
    "sensor.statistics.force_publish",
    ForcePublishAction,
    automation.maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(StatisticsComponent),
        }
    ),
)
async def sensor_statistics_reset_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


async def sensor_statistics_force_publish_to_code(
    config, action_id, template_arg, args
):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)
