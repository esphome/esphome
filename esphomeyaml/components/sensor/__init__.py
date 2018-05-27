import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import automation
from esphomeyaml.const import CONF_ABOVE, CONF_ACCURACY_DECIMALS, CONF_ALPHA, CONF_BELOW, \
    CONF_DEBOUNCE, CONF_DELTA, CONF_EXPIRE_AFTER, CONF_EXPONENTIAL_MOVING_AVERAGE, CONF_FILTERS, \
    CONF_FILTER_NAN, CONF_FILTER_OUT, CONF_HEARTBEAT, CONF_ICON, CONF_ID, CONF_LAMBDA, \
    CONF_MQTT_ID, CONF_MULTIPLY, CONF_NAME, CONF_OFFSET, CONF_ON_RAW_VALUE, CONF_ON_VALUE,\
    CONF_ON_VALUE_RANGE, CONF_OR, CONF_SEND_EVERY, CONF_SLIDING_WINDOW_MOVING_AVERAGE, \
    CONF_THROTTLE, CONF_TRIGGER_ID, CONF_UNIQUE, CONF_UNIT_OF_MEASUREMENT, CONF_WINDOW_SIZE
from esphomeyaml.helpers import App, ArrayInitializer, Pvariable, add, esphomelib_ns, float_, \
    process_lambda, setup_mqtt_component, templatable

PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({

})


def validate_recursive_filter(value):
    return FILTERS_SCHEMA(value)


FILTER_KEYS = [CONF_OFFSET, CONF_MULTIPLY, CONF_FILTER_OUT, CONF_FILTER_NAN,
               CONF_SLIDING_WINDOW_MOVING_AVERAGE, CONF_EXPONENTIAL_MOVING_AVERAGE, CONF_LAMBDA,
               CONF_THROTTLE, CONF_DELTA, CONF_UNIQUE, CONF_HEARTBEAT, CONF_DEBOUNCE, CONF_OR]

FILTERS_SCHEMA = vol.All(cv.ensure_list, [vol.All({
    vol.Optional(CONF_OFFSET): vol.Coerce(float),
    vol.Optional(CONF_MULTIPLY): vol.Coerce(float),
    vol.Optional(CONF_FILTER_OUT): vol.Coerce(float),
    vol.Optional(CONF_FILTER_NAN): None,
    vol.Optional(CONF_SLIDING_WINDOW_MOVING_AVERAGE): vol.Schema({
        vol.Required(CONF_WINDOW_SIZE): cv.positive_not_null_int,
        vol.Required(CONF_SEND_EVERY): cv.positive_not_null_int,
    }),
    vol.Optional(CONF_EXPONENTIAL_MOVING_AVERAGE): vol.Schema({
        vol.Required(CONF_ALPHA): cv.positive_float,
        vol.Required(CONF_SEND_EVERY): cv.positive_not_null_int,
    }),
    vol.Optional(CONF_LAMBDA): cv.lambda_,
    vol.Optional(CONF_THROTTLE): cv.positive_time_period_milliseconds,
    vol.Optional(CONF_DELTA): vol.Coerce(float),
    vol.Optional(CONF_UNIQUE): None,
    vol.Optional(CONF_HEARTBEAT): cv.positive_time_period_milliseconds,
    vol.Optional(CONF_DEBOUNCE): cv.positive_time_period_milliseconds,
    vol.Optional(CONF_OR): validate_recursive_filter,
}, cv.has_at_exactly_one_key(*FILTER_KEYS))])

SENSOR_SCHEMA = cv.MQTT_COMPONENT_SCHEMA.extend({
    cv.GenerateID('mqtt_sensor', CONF_MQTT_ID): cv.register_variable_id,
    cv.GenerateID('sensor'): cv.register_variable_id,
    vol.Required(CONF_NAME): cv.string,
    vol.Optional(CONF_UNIT_OF_MEASUREMENT): cv.string_strict,
    vol.Optional(CONF_ICON): cv.icon,
    vol.Optional(CONF_ACCURACY_DECIMALS): vol.Coerce(int),
    vol.Optional(CONF_EXPIRE_AFTER): vol.Any(None, cv.positive_time_period_milliseconds),
    vol.Optional(CONF_FILTERS): FILTERS_SCHEMA,
    vol.Optional(CONF_ON_VALUE): vol.All(cv.ensure_list, [automation.AUTOMATION_SCHEMA]),
    vol.Optional(CONF_ON_RAW_VALUE): vol.All(cv.ensure_list, [automation.AUTOMATION_SCHEMA]),
    vol.Optional(CONF_ON_VALUE_RANGE): vol.All(cv.ensure_list, [vol.All(
        automation.AUTOMATION_SCHEMA.extend({
            vol.Optional(CONF_ABOVE): vol.Coerce(float),
            vol.Optional(CONF_BELOW): vol.Coerce(float),
        }), cv.has_at_least_one_key(CONF_ABOVE, CONF_BELOW))]),
})

# pylint: disable=invalid-name
sensor_ns = esphomelib_ns.namespace('sensor')
Sensor = sensor_ns.Sensor
MQTTSensorComponent = sensor_ns.MQTTSensorComponent
OffsetFilter = sensor_ns.OffsetFilter
MultiplyFilter = sensor_ns.MultiplyFilter
FilterOutValueFilter = sensor_ns.FilterOutValueFilter
FilterOutNANFilter = sensor_ns.FilterOutNANFilter
SlidingWindowMovingAverageFilter = sensor_ns.SlidingWindowMovingAverageFilter
ExponentialMovingAverageFilter = sensor_ns.ExponentialMovingAverageFilter
LambdaFilter = sensor_ns.LambdaFilter
ThrottleFilter = sensor_ns.ThrottleFilter
DeltaFilter = sensor_ns.DeltaFilter
OrFilter = sensor_ns.OrFilter
HeartbeatFilter = sensor_ns.HeartbeatFilter
DebounceFilter = sensor_ns.DebounceFilter
UniqueFilter = sensor_ns.UniqueFilter

SensorValueTrigger = sensor_ns.SensorValueTrigger
RawSensorValueTrigger = sensor_ns.RawSensorValueTrigger
ValueRangeTrigger = sensor_ns.ValueRangeTrigger


def setup_filter(config):
    if CONF_OFFSET in config:
        return OffsetFilter.new(config[CONF_OFFSET])
    if CONF_MULTIPLY in config:
        return MultiplyFilter.new(config[CONF_MULTIPLY])
    if CONF_FILTER_OUT in config:
        return FilterOutValueFilter.new(config[CONF_FILTER_OUT])
    if CONF_FILTER_NAN in config:
        return FilterOutNANFilter()
    if CONF_SLIDING_WINDOW_MOVING_AVERAGE in config:
        conf = config[CONF_SLIDING_WINDOW_MOVING_AVERAGE]
        return SlidingWindowMovingAverageFilter.new(conf[CONF_WINDOW_SIZE], conf[CONF_SEND_EVERY])
    if CONF_EXPONENTIAL_MOVING_AVERAGE in config:
        conf = config[CONF_EXPONENTIAL_MOVING_AVERAGE]
        return ExponentialMovingAverageFilter.new(conf[CONF_ALPHA], conf[CONF_SEND_EVERY])
    if CONF_LAMBDA in config:
        return LambdaFilter.new(process_lambda(config[CONF_LAMBDA], [(float_, 'x')]))
    if CONF_THROTTLE in config:
        return ThrottleFilter.new(config[CONF_THROTTLE])
    if CONF_DELTA in config:
        return DeltaFilter.new(config[CONF_DELTA])
    if CONF_OR in config:
        return OrFilter.new(setup_filters(config[CONF_OR]))
    if CONF_HEARTBEAT in config:
        return App.register_component(HeartbeatFilter.new(config[CONF_HEARTBEAT]))
    if CONF_DEBOUNCE in config:
        return App.register_component(DebounceFilter.new(config[CONF_DEBOUNCE]))
    if CONF_UNIQUE in config:
        return UniqueFilter.new()
    raise ValueError(u"Filter unsupported: {}".format(config))


def setup_filters(config):
    return ArrayInitializer(*[setup_filter(x) for x in config])


def setup_sensor_core_(sensor_var, mqtt_var, config):
    if CONF_UNIT_OF_MEASUREMENT in config:
        add(sensor_var.set_unit_of_measurement(config[CONF_UNIT_OF_MEASUREMENT]))
    if CONF_ICON in config:
        add(sensor_var.set_icon(config[CONF_ICON]))
    if CONF_ACCURACY_DECIMALS in config:
        add(sensor_var.set_accuracy_decimals(config[CONF_ACCURACY_DECIMALS]))
    if CONF_FILTERS in config:
        add(sensor_var.set_filters(setup_filters(config[CONF_FILTERS])))

    for conf in config.get(CONF_ON_VALUE, []):
        rhs = sensor_var.make_value_trigger()
        trigger = Pvariable(SensorValueTrigger, conf[CONF_TRIGGER_ID], rhs)
        automation.build_automation(trigger, float_, conf)
    for conf in config.get(CONF_ON_RAW_VALUE, []):
        rhs = sensor_var.make_raw_value_trigger()
        trigger = Pvariable(RawSensorValueTrigger, conf[CONF_TRIGGER_ID], rhs)
        automation.build_automation(trigger, float_, conf)
    for conf in config.get(CONF_ON_VALUE_RANGE, []):
        rhs = sensor_var.make_value_range_trigger()
        trigger = Pvariable(ValueRangeTrigger, conf[CONF_TRIGGER_ID], rhs)
        if CONF_ABOVE in conf:
            trigger.set_min(templatable(conf[CONF_ABOVE], float_, float_))
        if CONF_BELOW in conf:
            trigger.set_max(templatable(conf[CONF_BELOW], float_, float_))
        automation.build_automation(trigger, float_, conf)

    if CONF_EXPIRE_AFTER in config:
        if config[CONF_EXPIRE_AFTER] is None:
            add(mqtt_var.disable_expire_after())
        else:
            add(mqtt_var.set_expire_after(config[CONF_EXPIRE_AFTER]))
    setup_mqtt_component(mqtt_var, config)


def setup_sensor(sensor_obj, mqtt_obj, config):
    sensor_var = Pvariable(Sensor, config[CONF_ID], sensor_obj, has_side_effects=False)
    mqtt_var = Pvariable(MQTTSensorComponent, config[CONF_MQTT_ID], mqtt_obj,
                         has_side_effects=False)
    setup_sensor_core_(sensor_var, mqtt_var, config)


def register_sensor(var, config):
    sensor_var = Pvariable(Sensor, config[CONF_ID], var, has_side_effects=True)
    rhs = App.register_sensor(sensor_var)
    mqtt_var = Pvariable(MQTTSensorComponent, config[CONF_MQTT_ID], rhs,
                         has_side_effects=True)
    setup_sensor_core_(sensor_var, mqtt_var, config)


BUILD_FLAGS = '-DUSE_SENSOR'
