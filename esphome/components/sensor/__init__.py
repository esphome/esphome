import math

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import mqtt
from esphome.const import (
    CONF_DEVICE_CLASS,
    CONF_ABOVE,
    CONF_ACCURACY_DECIMALS,
    CONF_ALPHA,
    CONF_BELOW,
    CONF_ENTITY_CATEGORY,
    CONF_EXPIRE_AFTER,
    CONF_FILTERS,
    CONF_FROM,
    CONF_ICON,
    CONF_ID,
    CONF_IGNORE_OUT_OF_RANGE,
    CONF_ON_RAW_VALUE,
    CONF_ON_VALUE,
    CONF_ON_VALUE_RANGE,
    CONF_QUANTILE,
    CONF_SEND_EVERY,
    CONF_SEND_FIRST_AT,
    CONF_STATE_CLASS,
    CONF_TIMEOUT,
    CONF_TO,
    CONF_TRIGGER_ID,
    CONF_TYPE,
    CONF_UNIT_OF_MEASUREMENT,
    CONF_WINDOW_SIZE,
    CONF_MQTT_ID,
    CONF_FORCE_UPDATE,
    CONF_VALUE,
    CONF_MIN_VALUE,
    CONF_MAX_VALUE,
    CONF_METHOD,
    DEVICE_CLASS_APPARENT_POWER,
    DEVICE_CLASS_AQI,
    DEVICE_CLASS_ATMOSPHERIC_PRESSURE,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_CARBON_MONOXIDE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_DATA_RATE,
    DEVICE_CLASS_DATA_SIZE,
    DEVICE_CLASS_DATE,
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_ENERGY_STORAGE,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_GAS,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_ILLUMINANCE,
    DEVICE_CLASS_IRRADIANCE,
    DEVICE_CLASS_MOISTURE,
    DEVICE_CLASS_MONETARY,
    DEVICE_CLASS_NITROGEN_DIOXIDE,
    DEVICE_CLASS_NITROGEN_MONOXIDE,
    DEVICE_CLASS_NITROUS_OXIDE,
    DEVICE_CLASS_OZONE,
    DEVICE_CLASS_PH,
    DEVICE_CLASS_PM1,
    DEVICE_CLASS_PM10,
    DEVICE_CLASS_PM25,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_POWER_FACTOR,
    DEVICE_CLASS_PRECIPITATION,
    DEVICE_CLASS_PRECIPITATION_INTENSITY,
    DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_REACTIVE_POWER,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    DEVICE_CLASS_SOUND_PRESSURE,
    DEVICE_CLASS_SPEED,
    DEVICE_CLASS_SULPHUR_DIOXIDE,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_TIMESTAMP,
    DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS,
    DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS_PARTS,
    DEVICE_CLASS_VOLTAGE,
    DEVICE_CLASS_VOLUME,
    DEVICE_CLASS_VOLUME_FLOW_RATE,
    DEVICE_CLASS_VOLUME_STORAGE,
    DEVICE_CLASS_WATER,
    DEVICE_CLASS_WEIGHT,
    DEVICE_CLASS_WIND_SPEED,
    ENTITY_CATEGORY_CONFIG,
)
from esphome.core import CORE, coroutine_with_priority
from esphome.cpp_generator import MockObjClass
from esphome.cpp_helpers import setup_entity
from esphome.util import Registry

CODEOWNERS = ["@esphome/core"]
DEVICE_CLASSES = [
    DEVICE_CLASS_APPARENT_POWER,
    DEVICE_CLASS_AQI,
    DEVICE_CLASS_ATMOSPHERIC_PRESSURE,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_CARBON_MONOXIDE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_DATA_RATE,
    DEVICE_CLASS_DATA_SIZE,
    DEVICE_CLASS_DATE,
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_ENERGY_STORAGE,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_GAS,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_ILLUMINANCE,
    DEVICE_CLASS_IRRADIANCE,
    DEVICE_CLASS_MOISTURE,
    DEVICE_CLASS_MONETARY,
    DEVICE_CLASS_NITROGEN_DIOXIDE,
    DEVICE_CLASS_NITROGEN_MONOXIDE,
    DEVICE_CLASS_NITROUS_OXIDE,
    DEVICE_CLASS_OZONE,
    DEVICE_CLASS_PH,
    DEVICE_CLASS_PM1,
    DEVICE_CLASS_PM10,
    DEVICE_CLASS_PM25,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_POWER_FACTOR,
    DEVICE_CLASS_PRECIPITATION,
    DEVICE_CLASS_PRECIPITATION_INTENSITY,
    DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_REACTIVE_POWER,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    DEVICE_CLASS_SOUND_PRESSURE,
    DEVICE_CLASS_SPEED,
    DEVICE_CLASS_SULPHUR_DIOXIDE,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_TIMESTAMP,
    DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS,
    DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS_PARTS,
    DEVICE_CLASS_VOLTAGE,
    DEVICE_CLASS_VOLUME,
    DEVICE_CLASS_VOLUME_FLOW_RATE,
    DEVICE_CLASS_VOLUME_STORAGE,
    DEVICE_CLASS_WATER,
    DEVICE_CLASS_WEIGHT,
    DEVICE_CLASS_WIND_SPEED,
]

sensor_ns = cg.esphome_ns.namespace("sensor")
StateClasses = sensor_ns.enum("StateClass")
STATE_CLASSES = {
    "": StateClasses.STATE_CLASS_NONE,
    "measurement": StateClasses.STATE_CLASS_MEASUREMENT,
    "total_increasing": StateClasses.STATE_CLASS_TOTAL_INCREASING,
    "total": StateClasses.STATE_CLASS_TOTAL,
}
validate_state_class = cv.enum(STATE_CLASSES, lower=True, space="_")

IS_PLATFORM_COMPONENT = True


def validate_send_first_at(value):
    send_first_at = value.get(CONF_SEND_FIRST_AT)
    send_every = value[CONF_SEND_EVERY]
    if send_first_at is not None and send_first_at > send_every:
        raise cv.Invalid(
            f"send_first_at must be smaller than or equal to send_every! {send_first_at} <= {send_every}"
        )
    return value


FILTER_REGISTRY = Registry()
validate_filters = cv.validate_registry("filter", FILTER_REGISTRY)


def validate_datapoint(value):
    if isinstance(value, dict):
        return cv.Schema(
            {
                cv.Required(CONF_FROM): cv.float_,
                cv.Required(CONF_TO): cv.float_,
            }
        )(value)
    value = cv.string(value)
    if "->" not in value:
        raise cv.Invalid("Datapoint mapping must contain '->'")
    a, b = value.split("->", 1)
    a, b = a.strip(), b.strip()
    return validate_datapoint({CONF_FROM: cv.float_(a), CONF_TO: cv.float_(b)})


_SENSOR_ENTITY_CATEGORIES = {
    k: v for k, v in cv.ENTITY_CATEGORIES.items() if k != ENTITY_CATEGORY_CONFIG
}


def sensor_entity_category(value):
    return cv.enum(_SENSOR_ENTITY_CATEGORIES, lower=True)(value)


# Base
Sensor = sensor_ns.class_("Sensor", cg.EntityBase)
SensorPtr = Sensor.operator("ptr")

# Triggers
SensorStateTrigger = sensor_ns.class_(
    "SensorStateTrigger", automation.Trigger.template(cg.float_)
)
SensorRawStateTrigger = sensor_ns.class_(
    "SensorRawStateTrigger", automation.Trigger.template(cg.float_)
)
ValueRangeTrigger = sensor_ns.class_(
    "ValueRangeTrigger", automation.Trigger.template(cg.float_), cg.Component
)
SensorPublishAction = sensor_ns.class_("SensorPublishAction", automation.Action)

# Filters
Filter = sensor_ns.class_("Filter")
QuantileFilter = sensor_ns.class_("QuantileFilter", Filter)
MedianFilter = sensor_ns.class_("MedianFilter", Filter)
SkipInitialFilter = sensor_ns.class_("SkipInitialFilter", Filter)
MinFilter = sensor_ns.class_("MinFilter", Filter)
MaxFilter = sensor_ns.class_("MaxFilter", Filter)
SlidingWindowMovingAverageFilter = sensor_ns.class_(
    "SlidingWindowMovingAverageFilter", Filter
)
ExponentialMovingAverageFilter = sensor_ns.class_(
    "ExponentialMovingAverageFilter", Filter
)
ThrottleAverageFilter = sensor_ns.class_("ThrottleAverageFilter", Filter, cg.Component)
LambdaFilter = sensor_ns.class_("LambdaFilter", Filter)
OffsetFilter = sensor_ns.class_("OffsetFilter", Filter)
MultiplyFilter = sensor_ns.class_("MultiplyFilter", Filter)
FilterOutValueFilter = sensor_ns.class_("FilterOutValueFilter", Filter)
ThrottleFilter = sensor_ns.class_("ThrottleFilter", Filter)
TimeoutFilter = sensor_ns.class_("TimeoutFilter", Filter, cg.Component)
DebounceFilter = sensor_ns.class_("DebounceFilter", Filter, cg.Component)
HeartbeatFilter = sensor_ns.class_("HeartbeatFilter", Filter, cg.Component)
DeltaFilter = sensor_ns.class_("DeltaFilter", Filter)
OrFilter = sensor_ns.class_("OrFilter", Filter)
CalibrateLinearFilter = sensor_ns.class_("CalibrateLinearFilter", Filter)
CalibratePolynomialFilter = sensor_ns.class_("CalibratePolynomialFilter", Filter)
SensorInRangeCondition = sensor_ns.class_("SensorInRangeCondition", Filter)
ClampFilter = sensor_ns.class_("ClampFilter", Filter)
RoundFilter = sensor_ns.class_("RoundFilter", Filter)

validate_unit_of_measurement = cv.string_strict
validate_accuracy_decimals = cv.int_
validate_icon = cv.icon
validate_device_class = cv.one_of(*DEVICE_CLASSES, lower=True, space="_")

SENSOR_SCHEMA = cv.ENTITY_BASE_SCHEMA.extend(cv.MQTT_COMPONENT_SCHEMA).extend(
    {
        cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(mqtt.MQTTSensorComponent),
        cv.GenerateID(): cv.declare_id(Sensor),
        cv.Optional(CONF_UNIT_OF_MEASUREMENT): validate_unit_of_measurement,
        cv.Optional(CONF_ACCURACY_DECIMALS): validate_accuracy_decimals,
        cv.Optional(CONF_DEVICE_CLASS): validate_device_class,
        cv.Optional(CONF_STATE_CLASS): validate_state_class,
        cv.Optional(CONF_ENTITY_CATEGORY): sensor_entity_category,
        cv.Optional("last_reset_type"): cv.invalid(
            "last_reset_type has been removed since 2021.9.0. state_class: total_increasing should be used for total values."
        ),
        cv.Optional(CONF_FORCE_UPDATE, default=False): cv.boolean,
        cv.Optional(CONF_EXPIRE_AFTER): cv.All(
            cv.requires_component("mqtt"),
            cv.Any(None, cv.positive_time_period_milliseconds),
        ),
        cv.Optional(CONF_FILTERS): validate_filters,
        cv.Optional(CONF_ON_VALUE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SensorStateTrigger),
            }
        ),
        cv.Optional(CONF_ON_RAW_VALUE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SensorRawStateTrigger),
            }
        ),
        cv.Optional(CONF_ON_VALUE_RANGE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ValueRangeTrigger),
                cv.Optional(CONF_ABOVE): cv.templatable(cv.float_),
                cv.Optional(CONF_BELOW): cv.templatable(cv.float_),
            },
            cv.has_at_least_one_key(CONF_ABOVE, CONF_BELOW),
        ),
    }
)

_UNDEF = object()


def sensor_schema(
    class_: MockObjClass = _UNDEF,
    *,
    unit_of_measurement: str = _UNDEF,
    icon: str = _UNDEF,
    accuracy_decimals: int = _UNDEF,
    device_class: str = _UNDEF,
    state_class: str = _UNDEF,
    entity_category: str = _UNDEF,
) -> cv.Schema:
    schema = {}

    if class_ is not _UNDEF:
        # Not optional.
        schema[cv.GenerateID()] = cv.declare_id(class_)

    for key, default, validator in [
        (CONF_UNIT_OF_MEASUREMENT, unit_of_measurement, validate_unit_of_measurement),
        (CONF_ICON, icon, validate_icon),
        (CONF_ACCURACY_DECIMALS, accuracy_decimals, validate_accuracy_decimals),
        (CONF_DEVICE_CLASS, device_class, validate_device_class),
        (CONF_STATE_CLASS, state_class, validate_state_class),
        (CONF_ENTITY_CATEGORY, entity_category, sensor_entity_category),
    ]:
        if default is not _UNDEF:
            schema[cv.Optional(key, default=default)] = validator

    return SENSOR_SCHEMA.extend(schema)


@FILTER_REGISTRY.register("offset", OffsetFilter, cv.float_)
async def offset_filter_to_code(config, filter_id):
    return cg.new_Pvariable(filter_id, config)


@FILTER_REGISTRY.register("multiply", MultiplyFilter, cv.float_)
async def multiply_filter_to_code(config, filter_id):
    return cg.new_Pvariable(filter_id, config)


@FILTER_REGISTRY.register("filter_out", FilterOutValueFilter, cv.float_)
async def filter_out_filter_to_code(config, filter_id):
    return cg.new_Pvariable(filter_id, config)


QUANTILE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_WINDOW_SIZE, default=5): cv.positive_not_null_int,
            cv.Optional(CONF_SEND_EVERY, default=5): cv.positive_not_null_int,
            cv.Optional(CONF_SEND_FIRST_AT, default=1): cv.positive_not_null_int,
            cv.Optional(CONF_QUANTILE, default=0.9): cv.zero_to_one_float,
        }
    ),
    validate_send_first_at,
)


@FILTER_REGISTRY.register("quantile", QuantileFilter, QUANTILE_SCHEMA)
async def quantile_filter_to_code(config, filter_id):
    return cg.new_Pvariable(
        filter_id,
        config[CONF_WINDOW_SIZE],
        config[CONF_SEND_EVERY],
        config[CONF_SEND_FIRST_AT],
        config[CONF_QUANTILE],
    )


MEDIAN_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_WINDOW_SIZE, default=5): cv.positive_not_null_int,
            cv.Optional(CONF_SEND_EVERY, default=5): cv.positive_not_null_int,
            cv.Optional(CONF_SEND_FIRST_AT, default=1): cv.positive_not_null_int,
        }
    ),
    validate_send_first_at,
)


@FILTER_REGISTRY.register("median", MedianFilter, MEDIAN_SCHEMA)
async def median_filter_to_code(config, filter_id):
    return cg.new_Pvariable(
        filter_id,
        config[CONF_WINDOW_SIZE],
        config[CONF_SEND_EVERY],
        config[CONF_SEND_FIRST_AT],
    )


MIN_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_WINDOW_SIZE, default=5): cv.positive_not_null_int,
            cv.Optional(CONF_SEND_EVERY, default=5): cv.positive_not_null_int,
            cv.Optional(CONF_SEND_FIRST_AT, default=1): cv.positive_not_null_int,
        }
    ),
    validate_send_first_at,
)


@FILTER_REGISTRY.register("skip_initial", SkipInitialFilter, cv.positive_not_null_int)
async def skip_initial_filter_to_code(config, filter_id):
    return cg.new_Pvariable(filter_id, config)


@FILTER_REGISTRY.register("min", MinFilter, MIN_SCHEMA)
async def min_filter_to_code(config, filter_id):
    return cg.new_Pvariable(
        filter_id,
        config[CONF_WINDOW_SIZE],
        config[CONF_SEND_EVERY],
        config[CONF_SEND_FIRST_AT],
    )


MAX_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_WINDOW_SIZE, default=5): cv.positive_not_null_int,
            cv.Optional(CONF_SEND_EVERY, default=5): cv.positive_not_null_int,
            cv.Optional(CONF_SEND_FIRST_AT, default=1): cv.positive_not_null_int,
        }
    ),
    validate_send_first_at,
)


@FILTER_REGISTRY.register("max", MaxFilter, MAX_SCHEMA)
async def max_filter_to_code(config, filter_id):
    return cg.new_Pvariable(
        filter_id,
        config[CONF_WINDOW_SIZE],
        config[CONF_SEND_EVERY],
        config[CONF_SEND_FIRST_AT],
    )


SLIDING_AVERAGE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_WINDOW_SIZE, default=15): cv.positive_not_null_int,
            cv.Optional(CONF_SEND_EVERY, default=15): cv.positive_not_null_int,
            cv.Optional(CONF_SEND_FIRST_AT, default=1): cv.positive_not_null_int,
        }
    ),
    validate_send_first_at,
)


@FILTER_REGISTRY.register(
    "sliding_window_moving_average",
    SlidingWindowMovingAverageFilter,
    SLIDING_AVERAGE_SCHEMA,
)
async def sliding_window_moving_average_filter_to_code(config, filter_id):
    return cg.new_Pvariable(
        filter_id,
        config[CONF_WINDOW_SIZE],
        config[CONF_SEND_EVERY],
        config[CONF_SEND_FIRST_AT],
    )


EXPONENTIAL_AVERAGE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_ALPHA, default=0.1): cv.positive_float,
            cv.Optional(CONF_SEND_EVERY, default=15): cv.positive_not_null_int,
            cv.Optional(CONF_SEND_FIRST_AT, default=1): cv.positive_not_null_int,
        }
    ),
    validate_send_first_at,
)


@FILTER_REGISTRY.register(
    "exponential_moving_average",
    ExponentialMovingAverageFilter,
    EXPONENTIAL_AVERAGE_SCHEMA,
)
async def exponential_moving_average_filter_to_code(config, filter_id):
    return cg.new_Pvariable(
        filter_id,
        config[CONF_ALPHA],
        config[CONF_SEND_EVERY],
        config[CONF_SEND_FIRST_AT],
    )


@FILTER_REGISTRY.register(
    "throttle_average", ThrottleAverageFilter, cv.positive_time_period_milliseconds
)
async def throttle_average_filter_to_code(config, filter_id):
    var = cg.new_Pvariable(filter_id, config)
    await cg.register_component(var, {})
    return var


@FILTER_REGISTRY.register("lambda", LambdaFilter, cv.returning_lambda)
async def lambda_filter_to_code(config, filter_id):
    lambda_ = await cg.process_lambda(
        config, [(float, "x")], return_type=cg.optional.template(float)
    )
    return cg.new_Pvariable(filter_id, lambda_)


DELTA_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_VALUE): cv.positive_float,
        cv.Optional(CONF_TYPE, default="absolute"): cv.one_of(
            "absolute", "percentage", lower=True
        ),
    }
)


def validate_delta(config):
    try:
        value = cv.positive_float(config)
        return DELTA_SCHEMA({CONF_VALUE: value, CONF_TYPE: "absolute"})
    except cv.Invalid:
        pass
    try:
        value = cv.percentage(config)
        return DELTA_SCHEMA({CONF_VALUE: value, CONF_TYPE: "percentage"})
    except cv.Invalid:
        pass
    raise cv.Invalid("Delta filter requires a positive number or percentage value.")


@FILTER_REGISTRY.register("delta", DeltaFilter, cv.Any(DELTA_SCHEMA, validate_delta))
async def delta_filter_to_code(config, filter_id):
    percentage = config[CONF_TYPE] == "percentage"
    return cg.new_Pvariable(
        filter_id,
        config[CONF_VALUE],
        percentage,
    )


@FILTER_REGISTRY.register("or", OrFilter, validate_filters)
async def or_filter_to_code(config, filter_id):
    filters = await build_filters(config)
    return cg.new_Pvariable(filter_id, filters)


@FILTER_REGISTRY.register(
    "throttle", ThrottleFilter, cv.positive_time_period_milliseconds
)
async def throttle_filter_to_code(config, filter_id):
    return cg.new_Pvariable(filter_id, config)


@FILTER_REGISTRY.register(
    "heartbeat", HeartbeatFilter, cv.positive_time_period_milliseconds
)
async def heartbeat_filter_to_code(config, filter_id):
    var = cg.new_Pvariable(filter_id, config)
    await cg.register_component(var, {})
    return var


TIMEOUT_SCHEMA = cv.maybe_simple_value(
    {
        cv.Required(CONF_TIMEOUT): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_VALUE, default="nan"): cv.float_,
    },
    key=CONF_TIMEOUT,
)


@FILTER_REGISTRY.register("timeout", TimeoutFilter, TIMEOUT_SCHEMA)
async def timeout_filter_to_code(config, filter_id):
    var = cg.new_Pvariable(filter_id, config[CONF_TIMEOUT], config[CONF_VALUE])
    await cg.register_component(var, {})
    return var


@FILTER_REGISTRY.register(
    "debounce", DebounceFilter, cv.positive_time_period_milliseconds
)
async def debounce_filter_to_code(config, filter_id):
    var = cg.new_Pvariable(filter_id, config)
    await cg.register_component(var, {})
    return var


CONF_DATAPOINTS = "datapoints"


def validate_calibrate_linear(config):
    datapoints = config[CONF_DATAPOINTS]
    if config[CONF_METHOD] == "exact":
        for i in range(len(datapoints) - 1):
            if datapoints[i][CONF_FROM] > datapoints[i + 1][CONF_FROM]:
                raise cv.Invalid(
                    "The 'from' values of the calibrate_linear filter must be sorted in ascending order."
                )
        for i in range(len(datapoints) - 1):
            if datapoints[i][CONF_FROM] == datapoints[i + 1][CONF_FROM]:
                raise cv.Invalid(
                    "The 'from' values of the calibrate_linear filter must not contain duplicates."
                )
    elif config[CONF_METHOD] == "least_squares":
        if all(conf[CONF_FROM] == datapoints[0][CONF_FROM] for conf in datapoints):
            raise cv.Invalid(
                "The 'from' values of the calibrate_linear filter cannot all point "
                "to the same value! Please add more values to the filter."
            )
    return config


@FILTER_REGISTRY.register(
    "calibrate_linear",
    CalibrateLinearFilter,
    cv.maybe_simple_value(
        {
            cv.Required(CONF_DATAPOINTS): cv.All(
                cv.ensure_list(validate_datapoint), cv.Length(min=2)
            ),
            cv.Optional(CONF_METHOD, default="least_squares"): cv.one_of(
                "least_squares", "exact", lower=True
            ),
        },
        validate_calibrate_linear,
        key=CONF_DATAPOINTS,
    ),
)
async def calibrate_linear_filter_to_code(config, filter_id):
    x = [conf[CONF_FROM] for conf in config[CONF_DATAPOINTS]]
    y = [conf[CONF_TO] for conf in config[CONF_DATAPOINTS]]

    linear_functions = []
    if config[CONF_METHOD] == "least_squares":
        k, b = fit_linear(x, y)
        linear_functions = [[k, b, float("NaN")]]
    elif config[CONF_METHOD] == "exact":
        linear_functions = map_linear(x, y)
    return cg.new_Pvariable(filter_id, linear_functions)


CONF_DEGREE = "degree"


def validate_calibrate_polynomial(config):
    if config[CONF_DEGREE] >= len(config[CONF_DATAPOINTS]):
        raise cv.Invalid(
            f"Degree is too high! Maximum possible degree with given datapoints is {len(config[CONF_DATAPOINTS]) - 1}",
            [CONF_DEGREE],
        )
    return config


@FILTER_REGISTRY.register(
    "calibrate_polynomial",
    CalibratePolynomialFilter,
    cv.All(
        cv.Schema(
            {
                cv.Required(CONF_DATAPOINTS): cv.All(
                    cv.ensure_list(validate_datapoint), cv.Length(min=1)
                ),
                cv.Required(CONF_DEGREE): cv.positive_int,
            }
        ),
        validate_calibrate_polynomial,
    ),
)
async def calibrate_polynomial_filter_to_code(config, filter_id):
    x = [conf[CONF_FROM] for conf in config[CONF_DATAPOINTS]]
    y = [conf[CONF_TO] for conf in config[CONF_DATAPOINTS]]
    degree = config[CONF_DEGREE]
    a = [[1] + [x_ ** (i + 1) for i in range(degree)] for x_ in x]
    # Column vector
    b = [[v] for v in y]
    res = [v[0] for v in _lstsq(a, b)]
    return cg.new_Pvariable(filter_id, res)


def validate_clamp(config):
    if not math.isfinite(config[CONF_MIN_VALUE]) and not math.isfinite(
        config[CONF_MAX_VALUE]
    ):
        raise cv.Invalid("Either 'min_value' or 'max_value' must be set to a number.")
    if config[CONF_MIN_VALUE] > config[CONF_MAX_VALUE]:
        raise cv.Invalid("The 'min_value' must not be larger than the 'max_value'.")
    return config


CLAMP_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_MIN_VALUE, default="NaN"): cv.float_,
            cv.Optional(CONF_MAX_VALUE, default="NaN"): cv.float_,
            cv.Optional(CONF_IGNORE_OUT_OF_RANGE, default=False): cv.boolean,
        }
    ),
    validate_clamp,
)


@FILTER_REGISTRY.register("clamp", ClampFilter, CLAMP_SCHEMA)
async def clamp_filter_to_code(config, filter_id):
    return cg.new_Pvariable(
        filter_id,
        config[CONF_MIN_VALUE],
        config[CONF_MAX_VALUE],
        config[CONF_IGNORE_OUT_OF_RANGE],
    )


@FILTER_REGISTRY.register(
    "round",
    RoundFilter,
    cv.maybe_simple_value(
        {
            cv.Required(CONF_ACCURACY_DECIMALS): cv.uint8_t,
        },
        key=CONF_ACCURACY_DECIMALS,
    ),
)
async def round_filter_to_code(config, filter_id):
    return cg.new_Pvariable(
        filter_id,
        config[CONF_ACCURACY_DECIMALS],
    )


async def build_filters(config):
    return await cg.build_registry_list(FILTER_REGISTRY, config)


async def setup_sensor_core_(var, config):
    await setup_entity(var, config)

    if (device_class := config.get(CONF_DEVICE_CLASS)) is not None:
        cg.add(var.set_device_class(device_class))
    if (state_class := config.get(CONF_STATE_CLASS)) is not None:
        cg.add(var.set_state_class(state_class))
    if (unit_of_measurement := config.get(CONF_UNIT_OF_MEASUREMENT)) is not None:
        cg.add(var.set_unit_of_measurement(unit_of_measurement))
    if (accuracy_decimals := config.get(CONF_ACCURACY_DECIMALS)) is not None:
        cg.add(var.set_accuracy_decimals(accuracy_decimals))
    cg.add(var.set_force_update(config[CONF_FORCE_UPDATE]))
    if config.get(CONF_FILTERS):  # must exist and not be empty
        filters = await build_filters(config[CONF_FILTERS])
        cg.add(var.set_filters(filters))

    for conf in config.get(CONF_ON_VALUE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(float, "x")], conf)
    for conf in config.get(CONF_ON_RAW_VALUE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(float, "x")], conf)
    for conf in config.get(CONF_ON_VALUE_RANGE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await cg.register_component(trigger, conf)
        if (above := conf.get(CONF_ABOVE)) is not None:
            template_ = await cg.templatable(above, [(float, "x")], float)
            cg.add(trigger.set_min(template_))
        if (below := conf.get(CONF_BELOW)) is not None:
            template_ = await cg.templatable(below, [(float, "x")], float)
            cg.add(trigger.set_max(template_))
        await automation.build_automation(trigger, [(float, "x")], conf)

    if (mqtt_id := config.get(CONF_MQTT_ID)) is not None:
        mqtt_ = cg.new_Pvariable(mqtt_id, var)
        await mqtt.register_mqtt_component(mqtt_, config)

        if (expire_after := config.get(CONF_EXPIRE_AFTER, _UNDEF)) is not _UNDEF:
            if expire_after is None:
                cg.add(mqtt_.disable_expire_after())
            else:
                cg.add(mqtt_.set_expire_after(expire_after))


async def register_sensor(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.register_sensor(var))
    await setup_sensor_core_(var, config)


async def new_sensor(config, *args):
    var = cg.new_Pvariable(config[CONF_ID], *args)
    await register_sensor(var, config)
    return var


SENSOR_IN_RANGE_CONDITION_SCHEMA = cv.All(
    {
        cv.Required(CONF_ID): cv.use_id(Sensor),
        cv.Optional(CONF_ABOVE): cv.float_,
        cv.Optional(CONF_BELOW): cv.float_,
    },
    cv.has_at_least_one_key(CONF_ABOVE, CONF_BELOW),
)


@automation.register_condition(
    "sensor.in_range", SensorInRangeCondition, SENSOR_IN_RANGE_CONDITION_SCHEMA
)
async def sensor_in_range_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(condition_id, template_arg, paren)

    if (above := config.get(CONF_ABOVE)) is not None:
        cg.add(var.set_min(above))
    if (below := config.get(CONF_BELOW)) is not None:
        cg.add(var.set_max(below))

    return var


def _mean(xs):
    return sum(xs) / len(xs)


def _std(x):
    return math.sqrt(sum((x_ - _mean(x)) ** 2 for x_ in x) / (len(x) - 1))


def _correlation_coeff(x, y):
    m_x, m_y = _mean(x), _mean(y)
    s_xy = sum((x_ - m_x) * (y_ - m_y) for x_, y_ in zip(x, y))
    s_sq_x = sum((x_ - m_x) ** 2 for x_ in x)
    s_sq_y = sum((y_ - m_y) ** 2 for y_ in y)
    return s_xy / math.sqrt(s_sq_x * s_sq_y)


def fit_linear(x, y):
    assert len(x) == len(y)
    m_x, m_y = _mean(x), _mean(y)
    r = _correlation_coeff(x, y)
    k = r * (_std(y) / _std(x))
    b = m_y - k * m_x
    return k, b


def map_linear(x, y):
    assert len(x) == len(y)
    f = []
    for i in range(len(x) - 1):
        slope = (y[i + 1] - y[i]) / (x[i + 1] - x[i])
        bias = y[i] - (slope * x[i])
        next_x = x[i + 1]
        if i == len(x) - 2:
            next_x = float("NaN")
        if f and f[-1][0] == slope and f[-1][1] == bias:
            f[-1][2] = next_x
        else:
            f.append([slope, bias, next_x])
    return f


def _mat_copy(m):
    return [list(row) for row in m]


def _mat_transpose(m):
    return _mat_copy(zip(*m))


def _mat_identity(n):
    return [[int(i == j) for j in range(n)] for i in range(n)]


def _mat_dot(a, b):
    b_t = _mat_transpose(b)
    return [[sum(x * y for x, y in zip(row_a, col_b)) for col_b in b_t] for row_a in a]


def _mat_inverse(m):
    n = len(m)
    m = _mat_copy(m)
    id = _mat_identity(n)

    for diag in range(n):
        # If diag element is 0, swap rows
        if m[diag][diag] == 0:
            for i in range(diag + 1, n):
                if m[i][diag] != 0:
                    break
            else:
                raise ValueError("Singular matrix, inverse cannot be calculated!")

            # Swap rows
            m[diag], m[i] = m[i], m[diag]
            id[diag], id[i] = id[i], id[diag]

        # Scale row to 1 in diagonal
        scaler = 1.0 / m[diag][diag]
        for j in range(n):
            m[diag][j] *= scaler
            id[diag][j] *= scaler

        # Subtract diag row
        for i in range(n):
            if i == diag:
                continue
            scaler = m[i][diag]
            for j in range(n):
                m[i][j] -= scaler * m[diag][j]
                id[i][j] -= scaler * id[diag][j]

    return id


def _lstsq(a, b):
    # min_x ||b - ax||^2_2 => x = (a^T a)^{-1} a^T b
    a_t = _mat_transpose(a)
    x = _mat_inverse(_mat_dot(a_t, a))
    return _mat_dot(_mat_dot(x, a_t), b)


@coroutine_with_priority(40.0)
async def to_code(config):
    cg.add_define("USE_SENSOR")
    cg.add_global(sensor_ns.using)
