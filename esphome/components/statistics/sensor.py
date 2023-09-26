import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome import automation
from esphome.core import CORE, Lambda
from esphome.components import sensor, time
from esphome.const import (
    CONF_ACCURACY_DECIMALS,
    CONF_COUNT,
    CONF_DEVICE_CLASS,
    CONF_DURATION,
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_ID,
    CONF_LAMBDA,
    CONF_RESTORE,
    CONF_SEND_EVERY,
    CONF_SEND_FIRST_AT,
    CONF_SOURCE_ID,
    CONF_TIME_ID,
    CONF_TRIGGER_ID,
    CONF_TYPE,
    CONF_UNIT_OF_MEASUREMENT,
    CONF_WINDOW_SIZE,
    DEVICE_CLASS_DURATION,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_MILLISECOND,
    UNIT_SECOND,
    UNIT_MINUTE,
    UNIT_HOUR,
)


CODEOWNERS = ["@kahrendt"]
DEPENDENCIES = ["time"]


# Borrowed from ili9xxx/display.py, accessed August 2023
def AUTO_LOAD():
    if CORE.is_esp32:
        return ["psram"]
    return []


statistics_ns = cg.esphome_ns.namespace("statistics")
StatisticsComponent = statistics_ns.class_("StatisticsComponent", cg.Component)

Aggregate = statistics_ns.class_("Aggregate")

###############
# Automations #
###############

CONF_ON_UPDATE = "on_update"

# Trigger for after statistics sensors are updated
StatisticsUpdateTrigger = statistics_ns.class_(
    "StatisticsUpdateTrigger", automation.Trigger.template(Aggregate)
)

# Force all sensors to publish
ForcePublishAction = statistics_ns.class_("ForcePublishAction", automation.Action)


# Reset action that clears all queued aggragates
ResetAction = statistics_ns.class_("ResetAction", automation.Action)


#####################
# Definable sensors #
#####################

CONF_MAX = "max"
CONF_MEAN = "mean"
CONF_MIN = "min"
CONF_SINCE_ARGMAX = "since_argmax"
CONF_SINCE_ARGMIN = "since_argmin"
CONF_QUADRATURE = "quadrature"
CONF_STATISTICS = "statistics"
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

####################################
# Configuration Options for Chunks #
####################################

CONF_CHUNK_SIZE = "chunk_size"
CONF_CHUNK_DURATION = "chunk_duration"

########################################
# Statistics Calculation Configuration #
########################################

# Struct stores group type and weight type
StatisticsCalculationConfig = statistics_ns.struct("StatisticsCalculationConfig")

# Group Type
CONF_GROUP_TYPE = "group_type"
GroupType = statistics_ns.enum("GroupType")
GROUP_TYPES = {
    "sample": GroupType.GROUP_TYPE_SAMPLE,
    "population": GroupType.GROUP_TYPE_POPULATION,
}

# Weight Type
CONF_WEIGHT_TYPE = "weight_type"
WeightType = statistics_ns.enum("WeightTye")
WEIGHT_TYPES = {
    "simple": WeightType.WEIGHT_TYPE_SIMPLE,
    "duration": WeightType.WEIGHT_TYPE_DURATION,
}


####################################################
# Time Unit Conversion for Sensors with Time Units #
####################################################

CONF_TIME_UNIT = "time_unit"

UNIT_DAY = "d"

TIME_CONVERSION_FACTORS = {
    UNIT_MILLISECOND: 1.0,
    UNIT_SECOND: 1000.0,
    UNIT_MINUTE: 60000.0,
    UNIT_HOUR: 3600000.0,
    UNIT_DAY: 86400000.0,
}


#####################################################
# Transformation Functions for Inherited Properties #
#####################################################


# Trend's unit is in original unit of measurement divides by time unit of measurement
def transform_trend_unit_of_measurement(time_unit, uom, config):
    return uom + "/" + time_unit


# Quadrature's unit is in original unit of measurement multiplied by the time unit of measurement
def transform_quadrature_unit_of_measurement(time_unit, uom, config):
    return uom + time_unit


# Increases accuracy decimals by 2
# Borrowed from sensor/integration/sensor.py (accessed July 2023)
def transform_accuracy_decimals(decimals, config):
    return decimals + 2


#######################################
# Raw Statistics Stored in Aggregates #
#######################################

TrackedStatisticsConfiguration = statistics_ns.struct("TrackedStatisticsConfiguration")

RAW_STAT_ARGMAX = "argmax"
RAW_STAT_ARGMIN = "argmin"
RAW_STAT_C2 = "c2"
RAW_STAT_DURATION = "duration"
RAW_STAT_DURATION_SQUARED = "duration_squared"
RAW_STAT_M2 = "m2"
RAW_STAT_MAX = "max"
RAW_STAT_MEAN = "mean"
RAW_STAT_MIN = "min"
RAW_STAT_TIMESTAMP_M2 = "timestamp_m2"
RAW_STAT_TIMESTAMP_MEAN = "timestamp_mean"
RAW_STAT_TIMESTAMP_REFERENCE = "timestamp_reference"


#################################
# Statistic Type Configurations #
#################################

StatisticType = statistics_ns.enum("StatisticType")

# Lambda code that returns the statistic from the Aggregate agg object
STAT_TYPE_CONF_RETURN_LAMBDA = "return_lambda"

# List of properties to inherit from source sensor
STAT_TYPE_CONF_INHERITED_PROPERTIES = "inhertited_properties"

# Function that transforms the number of accuracy decimals when inherited
STAT_TYPE_CONF_ACCURACY_DECIMALS_TRANSFORM = "accuracy_decimals_transform"

# Function that transforms the unit of measurement when inherited
STAT_TYPE_CONF_UOM_TRANSFORM = "uom_transform"

# Set of raw stats required to be tracked to compute the statistic
STAT_TYPE_CONF_REQUIRED_RAW_STATS = "required_raw_stats"

# The enum type of the sensor, used for logging
STAT_TYPE_CONF_ENUM_TYPE = "enum_type"

# The sensor schema used for defining the statistic type
STAT_TYPE_CONF_SENSOR_SCHEMA = "sensor_schema"

STATISTIC_TYPE_CONFIGS = {
    CONF_COUNT: {
        STAT_TYPE_CONF_RETURN_LAMBDA: "agg.get_count()",
        STAT_TYPE_CONF_INHERITED_PROPERTIES: [
            CONF_ENTITY_CATEGORY,
        ],
        STAT_TYPE_CONF_ACCURACY_DECIMALS_TRANSFORM: None,
        STAT_TYPE_CONF_UOM_TRANSFORM: None,
        STAT_TYPE_CONF_REQUIRED_RAW_STATS: {},
        STAT_TYPE_CONF_ENUM_TYPE: StatisticType.STATISTIC_COUNT,
        STAT_TYPE_CONF_SENSOR_SCHEMA: sensor.sensor_schema(
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
    },
    CONF_DURATION: {
        STAT_TYPE_CONF_RETURN_LAMBDA: "agg.get_duration()/{time_conversion}",
        STAT_TYPE_CONF_INHERITED_PROPERTIES: [
            CONF_ENTITY_CATEGORY,
            CONF_UNIT_OF_MEASUREMENT,
        ],
        STAT_TYPE_CONF_ACCURACY_DECIMALS_TRANSFORM: None,
        STAT_TYPE_CONF_UOM_TRANSFORM: CONF_TIME_UNIT,
        STAT_TYPE_CONF_REQUIRED_RAW_STATS: {RAW_STAT_DURATION},
        STAT_TYPE_CONF_ENUM_TYPE: StatisticType.STATISTIC_DURATION,
        STAT_TYPE_CONF_SENSOR_SCHEMA: sensor.sensor_schema(
            state_class=STATE_CLASS_MEASUREMENT,
            device_class=DEVICE_CLASS_DURATION,
        ).extend(
            {
                cv.Optional(CONF_TIME_UNIT, default=UNIT_SECOND): cv.enum(
                    TIME_CONVERSION_FACTORS, lower=True
                ),
            }
        ),
    },
    CONF_LAMBDA: {
        STAT_TYPE_CONF_INHERITED_PROPERTIES: [
            CONF_ENTITY_CATEGORY,
        ],
        STAT_TYPE_CONF_ACCURACY_DECIMALS_TRANSFORM: None,
        STAT_TYPE_CONF_UOM_TRANSFORM: None,
        STAT_TYPE_CONF_REQUIRED_RAW_STATS: {},
        STAT_TYPE_CONF_ENUM_TYPE: StatisticType.STATISTIC_LAMBDA,
        STAT_TYPE_CONF_SENSOR_SCHEMA: sensor.sensor_schema().extend(
            {
                cv.Required(CONF_LAMBDA): cv.returning_lambda,
            }
        ),
    },
    CONF_MAX: {
        STAT_TYPE_CONF_RETURN_LAMBDA: "agg.get_max()",
        STAT_TYPE_CONF_INHERITED_PROPERTIES: [
            CONF_ACCURACY_DECIMALS,
            CONF_DEVICE_CLASS,
            CONF_ENTITY_CATEGORY,
            CONF_ICON,
            CONF_UNIT_OF_MEASUREMENT,
        ],
        STAT_TYPE_CONF_ACCURACY_DECIMALS_TRANSFORM: None,
        STAT_TYPE_CONF_UOM_TRANSFORM: None,
        STAT_TYPE_CONF_REQUIRED_RAW_STATS: {RAW_STAT_MAX},
        STAT_TYPE_CONF_ENUM_TYPE: StatisticType.STATISTIC_MAX,
        STAT_TYPE_CONF_SENSOR_SCHEMA: sensor.sensor_schema(
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    },
    CONF_MEAN: {
        STAT_TYPE_CONF_RETURN_LAMBDA: "agg.get_mean()",
        STAT_TYPE_CONF_INHERITED_PROPERTIES: [
            CONF_ACCURACY_DECIMALS,
            CONF_DEVICE_CLASS,
            CONF_ENTITY_CATEGORY,
            CONF_ICON,
            CONF_UNIT_OF_MEASUREMENT,
        ],
        STAT_TYPE_CONF_ACCURACY_DECIMALS_TRANSFORM: None,
        STAT_TYPE_CONF_UOM_TRANSFORM: None,
        STAT_TYPE_CONF_REQUIRED_RAW_STATS: {RAW_STAT_MEAN},
        STAT_TYPE_CONF_ENUM_TYPE: StatisticType.STATISTIC_MEAN,
        STAT_TYPE_CONF_SENSOR_SCHEMA: sensor.sensor_schema(
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    },
    CONF_MIN: {
        STAT_TYPE_CONF_RETURN_LAMBDA: "agg.get_min()",
        STAT_TYPE_CONF_INHERITED_PROPERTIES: [
            CONF_ACCURACY_DECIMALS,
            CONF_DEVICE_CLASS,
            CONF_ENTITY_CATEGORY,
            CONF_ICON,
            CONF_UNIT_OF_MEASUREMENT,
        ],
        STAT_TYPE_CONF_ACCURACY_DECIMALS_TRANSFORM: None,
        STAT_TYPE_CONF_UOM_TRANSFORM: None,
        STAT_TYPE_CONF_REQUIRED_RAW_STATS: {RAW_STAT_MIN},
        STAT_TYPE_CONF_ENUM_TYPE: StatisticType.STATISTIC_MIN,
        STAT_TYPE_CONF_SENSOR_SCHEMA: sensor.sensor_schema(
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    },
    CONF_QUADRATURE: {
        STAT_TYPE_CONF_RETURN_LAMBDA: "agg.compute_quadrature()/{time_conversion}",
        STAT_TYPE_CONF_INHERITED_PROPERTIES: [
            CONF_ACCURACY_DECIMALS,
            CONF_ENTITY_CATEGORY,
            CONF_UNIT_OF_MEASUREMENT,
        ],
        STAT_TYPE_CONF_ACCURACY_DECIMALS_TRANSFORM: transform_accuracy_decimals,
        STAT_TYPE_CONF_UOM_TRANSFORM: transform_quadrature_unit_of_measurement,
        STAT_TYPE_CONF_REQUIRED_RAW_STATS: {RAW_STAT_DURATION, RAW_STAT_MEAN},
        STAT_TYPE_CONF_ENUM_TYPE: StatisticType.STATISTIC_QUADRATURE,
        STAT_TYPE_CONF_SENSOR_SCHEMA: sensor.sensor_schema(
            state_class=STATE_CLASS_TOTAL,
        ).extend(
            {
                cv.Optional(CONF_TIME_UNIT, default=UNIT_SECOND): cv.enum(
                    TIME_CONVERSION_FACTORS, lower=True
                ),
            }
        ),
    },
    CONF_SINCE_ARGMAX: {
        STAT_TYPE_CONF_RETURN_LAMBDA: "({time_component}->timestamp_now() - agg.get_argmax())*1000.0/{time_conversion}",
        STAT_TYPE_CONF_INHERITED_PROPERTIES: [
            CONF_ENTITY_CATEGORY,
            CONF_UNIT_OF_MEASUREMENT,
        ],
        STAT_TYPE_CONF_ACCURACY_DECIMALS_TRANSFORM: None,
        STAT_TYPE_CONF_UOM_TRANSFORM: CONF_TIME_UNIT,
        STAT_TYPE_CONF_REQUIRED_RAW_STATS: {RAW_STAT_ARGMAX, RAW_STAT_MAX},
        STAT_TYPE_CONF_ENUM_TYPE: StatisticType.STATISTIC_SINCE_ARGMAX,
        STAT_TYPE_CONF_SENSOR_SCHEMA: sensor.sensor_schema(
            state_class=STATE_CLASS_MEASUREMENT,
            device_class=DEVICE_CLASS_DURATION,
        ).extend(
            {
                cv.Optional(CONF_TIME_UNIT, default=UNIT_SECOND): cv.enum(
                    TIME_CONVERSION_FACTORS, lower=True
                ),
            }
        ),
    },
    CONF_SINCE_ARGMIN: {
        STAT_TYPE_CONF_RETURN_LAMBDA: "({time_component}->timestamp_now() - agg.get_argmin())*1000.0/{time_conversion}",
        STAT_TYPE_CONF_INHERITED_PROPERTIES: [
            CONF_ENTITY_CATEGORY,
            CONF_UNIT_OF_MEASUREMENT,
        ],
        STAT_TYPE_CONF_ACCURACY_DECIMALS_TRANSFORM: None,
        STAT_TYPE_CONF_UOM_TRANSFORM: CONF_TIME_UNIT,
        STAT_TYPE_CONF_REQUIRED_RAW_STATS: {RAW_STAT_ARGMIN, RAW_STAT_MIN},
        STAT_TYPE_CONF_ENUM_TYPE: StatisticType.STATISTIC_SINCE_ARGMIN,
        STAT_TYPE_CONF_SENSOR_SCHEMA: sensor.sensor_schema(
            state_class=STATE_CLASS_MEASUREMENT,
            device_class=DEVICE_CLASS_DURATION,
        ).extend(
            {
                cv.Optional(CONF_TIME_UNIT, default=UNIT_SECOND): cv.enum(
                    TIME_CONVERSION_FACTORS, lower=True
                ),
            }
        ),
    },
    CONF_STD_DEV: {
        STAT_TYPE_CONF_RETURN_LAMBDA: "agg.compute_std_dev()",
        STAT_TYPE_CONF_INHERITED_PROPERTIES: [
            CONF_ACCURACY_DECIMALS,
            CONF_DEVICE_CLASS,
            CONF_ENTITY_CATEGORY,
            CONF_ICON,
            CONF_UNIT_OF_MEASUREMENT,
        ],
        STAT_TYPE_CONF_ACCURACY_DECIMALS_TRANSFORM: transform_accuracy_decimals,
        STAT_TYPE_CONF_UOM_TRANSFORM: None,
        STAT_TYPE_CONF_REQUIRED_RAW_STATS: {RAW_STAT_M2, RAW_STAT_MEAN},
        STAT_TYPE_CONF_ENUM_TYPE: StatisticType.STATISTIC_STD_DEV,
        STAT_TYPE_CONF_SENSOR_SCHEMA: sensor.sensor_schema(
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    },
    CONF_TREND: {
        STAT_TYPE_CONF_RETURN_LAMBDA: "agg.compute_trend()*{time_conversion}",
        STAT_TYPE_CONF_INHERITED_PROPERTIES: [
            CONF_ACCURACY_DECIMALS,
            CONF_ENTITY_CATEGORY,
            CONF_UNIT_OF_MEASUREMENT,
        ],
        STAT_TYPE_CONF_ACCURACY_DECIMALS_TRANSFORM: transform_accuracy_decimals,
        STAT_TYPE_CONF_UOM_TRANSFORM: transform_trend_unit_of_measurement,
        STAT_TYPE_CONF_REQUIRED_RAW_STATS: {
            RAW_STAT_C2,
            RAW_STAT_M2,
            RAW_STAT_MEAN,
            RAW_STAT_TIMESTAMP_M2,
            RAW_STAT_TIMESTAMP_MEAN,
            RAW_STAT_TIMESTAMP_REFERENCE,
        },
        STAT_TYPE_CONF_ENUM_TYPE: StatisticType.STATISTIC_TREND,
        STAT_TYPE_CONF_SENSOR_SCHEMA: sensor.sensor_schema(
            state_class=STATE_CLASS_MEASUREMENT,
        ).extend(
            {
                cv.Optional(CONF_TIME_UNIT, default=UNIT_SECOND): cv.enum(
                    TIME_CONVERSION_FACTORS, lower=True
                ),
            }
        ),
    },
}


######################################
# Configuration Validation Functions #
######################################


# Borrowed from sensor/__init__.py (accessed July 2023)
def validate_send_first_at(config):
    send_first_at = config.get(CONF_SEND_FIRST_AT)
    send_every = config.get(CONF_SEND_EVERY)
    if send_every > 0:  # If send_every == 0, then automatic publication is disabled
        if send_first_at > send_every:
            raise cv.Invalid(
                f"send_first_at must be smaller than or equal to send_every! {send_first_at} <= {send_every}"
            )
    return config


# Ensures that the trend sensor is not enabled if restore from flash is enabled
def validate_no_trend_and_restore(config):
    window_config = config.get(CONF_WINDOW)

    if CONF_RESTORE in window_config:
        if stats_list := config.get(CONF_STATISTICS):
            for sens in stats_list:
                if sens.get("type") == CONF_TREND:
                    raise cv.Invalid(
                        "A trend sensor cannot be configured if restore is enabled."
                    )
    return config


# Based on inherit_property_from function from core/entity_helpers.py (accessed September 2023)
def set_property(property_to_inherit, property_value):
    """Validator that sets a provided configuration property, for use with FINAL_VALIDATE_SCHEMA.
    If a property is already set, it will not be inherited.
    Keyword arguments:
    property_to_inherit -- the name or path of the property to inherit, e.g. CONF_ICON or [CONF_SENSOR, 0, CONF_ICON]
                           (the parent must exist, otherwise nothing is done).
    property_value -- the value to set the property to
    """

    def _walk_config(config, path):
        walk = [path] if not isinstance(path, list) else path
        for item_or_index in walk:
            config = config[item_or_index]
        return config

    def inherit_property(config):
        # Split the property into its path and name
        if not isinstance(property_to_inherit, list):
            property_path, property = [], property_to_inherit
        else:
            property_path, property = property_to_inherit[:-1], property_to_inherit[-1]

        # Check if the property to inherit is accessible
        try:
            config_part = _walk_config(config, property_path)
        except KeyError:
            return config

        # Only inherit the property if it does not exist yet
        if property not in config_part:
            fconf = fv.full_config.get()

            path = fconf.get_path_for_id(config[CONF_ID])[:-1]

            this_config = _walk_config(fconf.get_config_for_path(path), property_path)

            this_config[property] = property_value
        return config

    return inherit_property


# Based on inherit_property_from function from core/entity_helpers.py (accessed September 2023)
def inherit_property_from_id(property_to_inherit, parent_id, transform=None):
    """Validator that inherits a configuration property from another entity, for use with FINAL_VALIDATE_SCHEMA.
    If a property is already set, it will not be inherited.
    Keyword arguments:
    property_to_inherit -- the name or path of the property to inherit, e.g. CONF_ICON or [CONF_SENSOR, 0, CONF_ICON]
                           (the parent must exist, otherwise nothing is done).
    parent_id -- the ID of the parent from which the property is inherited.
    """

    def _walk_config(config, path):
        walk = [path] if not isinstance(path, list) else path
        for item_or_index in walk:
            config = config[item_or_index]
        return config

    def inherit_property(config):
        # Split the property into its path and name
        if not isinstance(property_to_inherit, list):
            property_path, property = [], property_to_inherit
        else:
            property_path, property = property_to_inherit[:-1], property_to_inherit[-1]

        # Check if the property to inherit is accessible
        try:
            config_part = _walk_config(config, property_path)
        except KeyError:
            return config

        # Only inherit the property if it does not exist yet
        if property not in config_part:
            fconf = fv.full_config.get()

            # Get config for the parent entity
            # parent_id = _walk_config(config, parent_id_property)
            parent_path = fconf.get_path_for_id(parent_id)[:-1]
            parent_config = fconf.get_config_for_path(parent_path)

            # If parent sensor has the property set, inherit it
            if property in parent_config:
                path = fconf.get_path_for_id(config[CONF_ID])[:-1]
                this_config = _walk_config(
                    fconf.get_config_for_path(path), property_path
                )
                value = parent_config[property]
                if transform:
                    value = transform(value, config)
                this_config[property] = value

        return config

    return inherit_property


# In STATISTICS_TYPE_CONFIGS, the uom_transform function has an input of the time unit, but the transform function expected by the inherit_property_from_id function does not include it
def get_time_adjusted_uom(base_function, time_unit):
    if callable(base_function):
        return lambda uom, config: base_function(
            time_unit=time_unit,
            uom=uom,
            config=config,
        )

    return str(time_unit)


# Return a function that will inherit `property_to_inherit` from `source_id` for the statistic `stat_type` given the sensor's `config`
def get_property_inherit_function(config, stats_type, property_to_inherit, source_id):
    if property_to_inherit == CONF_UNIT_OF_MEASUREMENT:
        # Only statistics with a time unit change the unit of measurement
        if time_unit := config.get(CONF_TIME_UNIT):
            time_adjusted_uom = get_time_adjusted_uom(
                stats_type.get(STAT_TYPE_CONF_UOM_TRANSFORM), time_unit
            )

            # If time adjusted uom is a string, then set that as the unit
            if isinstance(time_adjusted_uom, str):
                return set_property(CONF_UNIT_OF_MEASUREMENT, time_adjusted_uom)
            # Otherewise, it is a transformation function
            return inherit_property_from_id(
                CONF_UNIT_OF_MEASUREMENT, source_id, transform=time_adjusted_uom
            )
    elif property_to_inherit == CONF_ACCURACY_DECIMALS:
        return inherit_property_from_id(
            CONF_ACCURACY_DECIMALS,
            source_id,
            transform=stats_type.get(STAT_TYPE_CONF_ACCURACY_DECIMALS_TRANSFORM),
        )

    return inherit_property_from_id(property_to_inherit, source_id)


# Sets/inherits properties based on each statistic type
def validate_statistic_sensor(config):
    fconf = fv.full_config.get()

    # Get ID for source sensor
    path = fconf.get_path_for_id(config[CONF_ID])[:-3]
    path.append(CONF_SOURCE_ID)
    source_id = fconf.get_config_for_path(path)

    stats_type = STATISTIC_TYPE_CONFIGS[config.get(CONF_TYPE)]

    for property_to_inherit in stats_type[STAT_TYPE_CONF_INHERITED_PROPERTIES]:
        inherit_function = get_property_inherit_function(
            config, stats_type, property_to_inherit, source_id
        )

        inherit_function(config)  # attempt to inherit the property

    return config


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
    cv.has_at_most_one_key(CONF_CHUNK_SIZE, CONF_CHUNK_DURATION),
)

CONTINUOUS_WINDOW_SCHEMA = cv.All(
    cv.Schema(
        {
            # A config value of 0 disables: window_size resets, send_every publications
            cv.Required(CONF_WINDOW_SIZE): cv.positive_int,
            cv.Optional(CONF_CHUNK_SIZE): cv.positive_not_null_int,
            cv.Optional(CONF_CHUNK_DURATION): cv.time_period,
            cv.Optional(CONF_SEND_EVERY, default=1): cv.positive_int,
            cv.Optional(CONF_SEND_FIRST_AT, default=1): cv.positive_not_null_int,
            cv.Optional(CONF_RESTORE): cv.boolean,
        },
    ),
    validate_send_first_at,
    cv.has_at_most_one_key(CONF_CHUNK_SIZE, CONF_CHUNK_DURATION),
)

SENSOR_TYPE_SCHEMA = cv.typed_schema(
    {
        CONF_SINCE_ARGMAX: STATISTIC_TYPE_CONFIGS[CONF_SINCE_ARGMAX][
            STAT_TYPE_CONF_SENSOR_SCHEMA
        ],
        CONF_SINCE_ARGMIN: STATISTIC_TYPE_CONFIGS[CONF_SINCE_ARGMIN][
            STAT_TYPE_CONF_SENSOR_SCHEMA
        ],
        CONF_COUNT: STATISTIC_TYPE_CONFIGS[CONF_COUNT][STAT_TYPE_CONF_SENSOR_SCHEMA],
        CONF_DURATION: STATISTIC_TYPE_CONFIGS[CONF_DURATION][
            STAT_TYPE_CONF_SENSOR_SCHEMA
        ],
        CONF_MAX: STATISTIC_TYPE_CONFIGS[CONF_MAX][STAT_TYPE_CONF_SENSOR_SCHEMA],
        CONF_MEAN: STATISTIC_TYPE_CONFIGS[CONF_MEAN][STAT_TYPE_CONF_SENSOR_SCHEMA],
        CONF_MIN: STATISTIC_TYPE_CONFIGS[CONF_MIN][STAT_TYPE_CONF_SENSOR_SCHEMA],
        CONF_QUADRATURE: STATISTIC_TYPE_CONFIGS[CONF_QUADRATURE][
            STAT_TYPE_CONF_SENSOR_SCHEMA
        ],
        CONF_STD_DEV: STATISTIC_TYPE_CONFIGS[CONF_STD_DEV][
            STAT_TYPE_CONF_SENSOR_SCHEMA
        ],
        CONF_TREND: STATISTIC_TYPE_CONFIGS[CONF_TREND][STAT_TYPE_CONF_SENSOR_SCHEMA],
        CONF_LAMBDA: STATISTIC_TYPE_CONFIGS[CONF_LAMBDA][STAT_TYPE_CONF_SENSOR_SCHEMA],
    }
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(StatisticsComponent),
            cv.GenerateID(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
            cv.Required(CONF_SOURCE_ID): cv.use_id(sensor.Sensor),
            cv.Required(CONF_WINDOW): cv.typed_schema(
                {
                    CONF_SLIDING: SLIDING_WINDOW_SCHEMA,
                    CONF_CONTINUOUS: CONTINUOUS_WINDOW_SCHEMA,
                    CONF_CONTINUOUS_LONG_TERM: CONTINUOUS_WINDOW_SCHEMA,
                }
            ),
            cv.Optional(CONF_WEIGHT_TYPE, default="simple"): cv.enum(
                WEIGHT_TYPES, lower=True
            ),
            cv.Optional(CONF_GROUP_TYPE, default="sample"): cv.enum(
                GROUP_TYPES, lower=True
            ),
            cv.Optional(CONF_STATISTICS): cv.ensure_list(SENSOR_TYPE_SCHEMA),
            cv.Optional(CONF_ON_UPDATE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        StatisticsUpdateTrigger
                    ),
                }
            ),
        },
    ).extend(cv.COMPONENT_SCHEMA),
    validate_no_trend_and_restore,
)

FINAL_VALIDATE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_STATISTICS): [validate_statistic_sensor],
    },
    extra=cv.ALLOW_EXTRA,
)

###################
# Code Generation #
###################


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    source = await cg.get_variable(config[CONF_SOURCE_ID])

    time_ = await cg.get_variable(config[CONF_TIME_ID])
    cg.add(var.set_time(time_))

    cg.add(var.set_hash(str(config[CONF_ID])))

    cg.add(var.set_source_sensor(source))

    cg.add(
        var.set_statistics_calculation_config(
            cg.StructInitializer(
                StatisticsCalculationConfig,
                ("group_type", config.get(CONF_GROUP_TYPE)),
                ("weight_type", config.get(CONF_WEIGHT_TYPE)),
            ),
        )
    )

    ####################
    # Configure Window #
    ####################
    window_config = config.get(CONF_WINDOW)
    window_type = WINDOW_TYPES[window_config.get(CONF_TYPE)]
    cg.add(var.set_window_type(window_type))

    # Set up window size
    window_size_setting = window_config.get(CONF_WINDOW_SIZE)
    if window_size_setting > 0:
        cg.add(var.set_window_size(window_size_setting))
    else:
        cg.add(var.set_window_size(4294967295))  # uint32_t max

    if chunk_size_setting := window_config.get(CONF_CHUNK_SIZE):
        cg.add(var.set_chunk_size(chunk_size_setting))
        cg.add(var.set_chunk_duration(4294967295))  # uint32_t max
    elif chunk_duration_setting := window_config.get(CONF_CHUNK_DURATION):
        cg.add(var.set_chunk_size(4294967295))  # uint32_t max
        cg.add(var.set_chunk_duration(chunk_duration_setting.total_milliseconds))
    else:
        # If neither CONF_CHUNK_SIZE or CONF_CHUNK_DURATION are configured, the default chunk size is 1
        cg.add(var.set_chunk_size(1))
        cg.add(var.set_chunk_duration(4294967295))  # uint32_t max

    # Set up send parameters
    cg.add(var.set_first_at(window_config.get(CONF_SEND_FIRST_AT)))

    send_every_setting = window_config.get(CONF_SEND_EVERY)
    if send_every_setting > 0:
        cg.add(var.set_send_every(send_every_setting))
    else:
        cg.add(var.set_send_every(4294967295))  # uint32_t max

    # Set up restore setting
    if restore_setting := window_config.get(CONF_RESTORE):
        cg.add(var.set_restore(restore_setting))

    # Set up triggers
    for conf in config.get(CONF_ON_UPDATE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(Aggregate, "agg")], conf)

    #############################
    # Set up Configured Sensors #
    #############################
    tracked_stats = set()

    if config.get(CONF_WEIGHT_TYPE) == "duration":
        tracked_stats.add(RAW_STAT_DURATION)
        if config.get(CONF_GROUP_TYPE) == "sample":
            # Squared duration is used for reliability weights
            # Only needed for sample group types that are duration weighted
            tracked_stats.add(RAW_STAT_DURATION_SQUARED)

    if stat_sensor_list := config.get(CONF_STATISTICS):
        for stat_sensor in stat_sensor_list:
            sens = await sensor.new_sensor(stat_sensor)

            if stat_sensor.get(CONF_TYPE) == CONF_LAMBDA:
                func = await cg.process_lambda(
                    stat_sensor[CONF_LAMBDA],
                    [(Aggregate, "agg")],
                    return_type=cg.float_,
                )
            else:
                # Substitute the sensor's configured time unit into the return lambda string if defined
                return_string = STATISTIC_TYPE_CONFIGS[stat_sensor.get(CONF_TYPE)][
                    STAT_TYPE_CONF_RETURN_LAMBDA
                ].format(
                    time_conversion=TIME_CONVERSION_FACTORS[
                        stat_sensor.get(CONF_TIME_UNIT)
                    ]
                    if (CONF_TIME_UNIT in stat_sensor)
                    else 1,
                    time_component=time_,
                )

                func = await cg.process_lambda(
                    Lambda(f"return {return_string};"),
                    [(Aggregate, "agg")],
                    return_type=cg.float_,
                )

            tracked_stats.update(
                STATISTIC_TYPE_CONFIGS[stat_sensor.get(CONF_TYPE)][
                    STAT_TYPE_CONF_REQUIRED_RAW_STATS
                ]
            )

            statistic_type = STATISTIC_TYPE_CONFIGS[stat_sensor.get(CONF_TYPE)][
                STAT_TYPE_CONF_ENUM_TYPE
            ]

            cg.add(
                var.add_sensor(
                    sens,
                    func,
                    statistic_type,
                )
            )

    # Sets which raw statistics are tracked based on the configured sensors
    cg.add(
        var.set_tracked_statistics(
            cg.StructInitializer(
                TrackedStatisticsConfiguration,
                (RAW_STAT_ARGMAX, RAW_STAT_ARGMAX in tracked_stats),
                (RAW_STAT_ARGMIN, RAW_STAT_ARGMIN in tracked_stats),
                (RAW_STAT_C2, RAW_STAT_C2 in tracked_stats),
                (RAW_STAT_DURATION, RAW_STAT_DURATION in tracked_stats),
                (RAW_STAT_DURATION_SQUARED, RAW_STAT_DURATION_SQUARED in tracked_stats),
                (RAW_STAT_M2, RAW_STAT_M2 in tracked_stats),
                (RAW_STAT_MAX, RAW_STAT_MAX in tracked_stats),
                (RAW_STAT_MEAN, RAW_STAT_MEAN in tracked_stats),
                (RAW_STAT_MIN, RAW_STAT_MIN in tracked_stats),
                (RAW_STAT_TIMESTAMP_M2, RAW_STAT_TIMESTAMP_M2 in tracked_stats),
                (RAW_STAT_TIMESTAMP_MEAN, RAW_STAT_TIMESTAMP_MEAN in tracked_stats),
                (
                    RAW_STAT_TIMESTAMP_REFERENCE,
                    RAW_STAT_TIMESTAMP_REFERENCE in tracked_stats,
                ),
            )
        )
    )


######################
# Automation Actions #
######################


@automation.register_action(
    "sensor.statistics.force_publish",
    ForcePublishAction,
    automation.maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(StatisticsComponent),
        }
    ),
)
async def sensor_statistics_force_publish_to_code(
    config, action_id, template_arg, args
):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "sensor.statistics.reset",
    ResetAction,
    automation.maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(StatisticsComponent),
        }
    ),
)
async def sensor_statistics_reset_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)
