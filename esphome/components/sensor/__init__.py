import voluptuous as vol

from esphome import automation
from esphome.automation import CONDITION_REGISTRY
from esphome.components import mqtt
from esphome.components.mqtt import setup_mqtt_component
import esphome.config_validation as cv
from esphome.const import CONF_ABOVE, CONF_ACCURACY_DECIMALS, CONF_ALPHA, CONF_BELOW, \
    CONF_DEBOUNCE, CONF_DELTA, CONF_EXPIRE_AFTER, CONF_EXPONENTIAL_MOVING_AVERAGE, CONF_FILTERS, \
    CONF_FILTER_NAN, CONF_FILTER_OUT, CONF_HEARTBEAT, CONF_ICON, CONF_ID, CONF_INTERNAL, \
    CONF_LAMBDA, CONF_MQTT_ID, CONF_MULTIPLY, CONF_OFFSET, CONF_ON_RAW_VALUE, CONF_ON_VALUE, \
    CONF_ON_VALUE_RANGE, CONF_OR, CONF_SEND_EVERY, CONF_SEND_FIRST_AT, \
    CONF_SLIDING_WINDOW_MOVING_AVERAGE, CONF_THROTTLE, CONF_TRIGGER_ID, CONF_UNIQUE, \
    CONF_UNIT_OF_MEASUREMENT, CONF_WINDOW_SIZE
from esphome.core import CORE
from esphome.cpp_generator import Pvariable, add, get_variable, process_lambda, templatable
from esphome.cpp_types import App, Component, Nameable, PollingComponent, Trigger, \
    esphome_ns, float_, optional

PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({

})


def validate_recursive_filter(value):
    return FILTERS_SCHEMA(value)


def validate_send_first_at(value):
    send_first_at = value.get(CONF_SEND_FIRST_AT)
    send_every = value[CONF_SEND_EVERY]
    if send_first_at is not None and send_first_at > send_every:
        raise vol.Invalid("send_first_at must be smaller than or equal to send_every! {} <= {}"
                          "".format(send_first_at, send_every))
    return value


FILTER_KEYS = [CONF_OFFSET, CONF_MULTIPLY, CONF_FILTER_OUT, CONF_FILTER_NAN,
               CONF_SLIDING_WINDOW_MOVING_AVERAGE, CONF_EXPONENTIAL_MOVING_AVERAGE, CONF_LAMBDA,
               CONF_THROTTLE, CONF_DELTA, CONF_UNIQUE, CONF_HEARTBEAT, CONF_DEBOUNCE, CONF_OR]

FILTERS_SCHEMA = cv.ensure_list({
    vol.Optional(CONF_OFFSET): cv.float_,
    vol.Optional(CONF_MULTIPLY): cv.float_,
    vol.Optional(CONF_FILTER_OUT): cv.float_,
    vol.Optional(CONF_FILTER_NAN): None,
    vol.Optional(CONF_SLIDING_WINDOW_MOVING_AVERAGE): vol.All(vol.Schema({
        vol.Optional(CONF_WINDOW_SIZE, default=15): cv.positive_not_null_int,
        vol.Optional(CONF_SEND_EVERY, default=15): cv.positive_not_null_int,
        vol.Optional(CONF_SEND_FIRST_AT): cv.positive_not_null_int,
    }), validate_send_first_at),
    vol.Optional(CONF_EXPONENTIAL_MOVING_AVERAGE): vol.Schema({
        vol.Optional(CONF_ALPHA, default=0.1): cv.positive_float,
        vol.Optional(CONF_SEND_EVERY, default=15): cv.positive_not_null_int,
    }),
    vol.Optional(CONF_LAMBDA): cv.lambda_,
    vol.Optional(CONF_THROTTLE): cv.positive_time_period_milliseconds,
    vol.Optional(CONF_DELTA): cv.float_,
    vol.Optional(CONF_UNIQUE): None,
    vol.Optional(CONF_HEARTBEAT): cv.positive_time_period_milliseconds,
    vol.Optional(CONF_DEBOUNCE): cv.positive_time_period_milliseconds,
    vol.Optional(CONF_OR): validate_recursive_filter,
}, cv.has_exactly_one_key(*FILTER_KEYS))

# Base
sensor_ns = esphome_ns.namespace('sensor')
Sensor = sensor_ns.class_('Sensor', Nameable)
SensorPtr = Sensor.operator('ptr')
MQTTSensorComponent = sensor_ns.class_('MQTTSensorComponent', mqtt.MQTTComponent)

PollingSensorComponent = sensor_ns.class_('PollingSensorComponent', PollingComponent, Sensor)
EmptySensor = sensor_ns.class_('EmptySensor', Sensor)
EmptyPollingParentSensor = sensor_ns.class_('EmptyPollingParentSensor', EmptySensor)

# Triggers
SensorStateTrigger = sensor_ns.class_('SensorStateTrigger', Trigger.template(float_))
SensorRawStateTrigger = sensor_ns.class_('SensorRawStateTrigger', Trigger.template(float_))
ValueRangeTrigger = sensor_ns.class_('ValueRangeTrigger', Trigger.template(float_), Component)

# Filters
Filter = sensor_ns.class_('Filter')
SlidingWindowMovingAverageFilter = sensor_ns.class_('SlidingWindowMovingAverageFilter', Filter)
ExponentialMovingAverageFilter = sensor_ns.class_('ExponentialMovingAverageFilter', Filter)
LambdaFilter = sensor_ns.class_('LambdaFilter', Filter)
OffsetFilter = sensor_ns.class_('OffsetFilter', Filter)
MultiplyFilter = sensor_ns.class_('MultiplyFilter', Filter)
FilterOutValueFilter = sensor_ns.class_('FilterOutValueFilter', Filter)
FilterOutNANFilter = sensor_ns.class_('FilterOutNANFilter', Filter)
ThrottleFilter = sensor_ns.class_('ThrottleFilter', Filter)
DebounceFilter = sensor_ns.class_('DebounceFilter', Filter, Component)
HeartbeatFilter = sensor_ns.class_('HeartbeatFilter', Filter, Component)
DeltaFilter = sensor_ns.class_('DeltaFilter', Filter)
OrFilter = sensor_ns.class_('OrFilter', Filter)
UniqueFilter = sensor_ns.class_('UniqueFilter', Filter)
SensorInRangeCondition = sensor_ns.class_('SensorInRangeCondition', Filter)

SENSOR_SCHEMA = cv.MQTT_COMPONENT_SCHEMA.extend({
    cv.GenerateID(CONF_MQTT_ID): cv.declare_variable_id(MQTTSensorComponent),
    vol.Optional(CONF_UNIT_OF_MEASUREMENT): cv.string_strict,
    vol.Optional(CONF_ICON): cv.icon,
    vol.Optional(CONF_ACCURACY_DECIMALS): vol.Coerce(int),
    vol.Optional(CONF_EXPIRE_AFTER): vol.All(cv.requires_component('mqtt'),
                                             vol.Any(None, cv.positive_time_period_milliseconds)),
    vol.Optional(CONF_FILTERS): FILTERS_SCHEMA,
    vol.Optional(CONF_ON_VALUE): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_variable_id(SensorStateTrigger),
    }),
    vol.Optional(CONF_ON_RAW_VALUE): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_variable_id(SensorRawStateTrigger),
    }),
    vol.Optional(CONF_ON_VALUE_RANGE): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_variable_id(ValueRangeTrigger),
        vol.Optional(CONF_ABOVE): cv.float_,
        vol.Optional(CONF_BELOW): cv.float_,
    }, cv.has_at_least_one_key(CONF_ABOVE, CONF_BELOW)),
})

SENSOR_PLATFORM_SCHEMA = PLATFORM_SCHEMA.extend(SENSOR_SCHEMA.schema)


def setup_filter(config):
    if CONF_OFFSET in config:
        yield OffsetFilter.new(config[CONF_OFFSET])
    elif CONF_MULTIPLY in config:
        yield MultiplyFilter.new(config[CONF_MULTIPLY])
    elif CONF_FILTER_OUT in config:
        yield FilterOutValueFilter.new(config[CONF_FILTER_OUT])
    elif CONF_FILTER_NAN in config:
        yield FilterOutNANFilter.new()
    elif CONF_SLIDING_WINDOW_MOVING_AVERAGE in config:
        conf = config[CONF_SLIDING_WINDOW_MOVING_AVERAGE]
        yield SlidingWindowMovingAverageFilter.new(conf[CONF_WINDOW_SIZE], conf[CONF_SEND_EVERY],
                                                   conf.get(CONF_SEND_FIRST_AT))
    elif CONF_EXPONENTIAL_MOVING_AVERAGE in config:
        conf = config[CONF_EXPONENTIAL_MOVING_AVERAGE]
        yield ExponentialMovingAverageFilter.new(conf[CONF_ALPHA], conf[CONF_SEND_EVERY])
    elif CONF_LAMBDA in config:
        for lambda_ in process_lambda(config[CONF_LAMBDA], [(float_, 'x')],
                                      return_type=optional.template(float_)):
            yield None
        yield LambdaFilter.new(lambda_)
    elif CONF_THROTTLE in config:
        yield ThrottleFilter.new(config[CONF_THROTTLE])
    elif CONF_DELTA in config:
        yield DeltaFilter.new(config[CONF_DELTA])
    elif CONF_OR in config:
        for filters in setup_filters(config[CONF_OR]):
            yield None
        yield OrFilter.new(filters)
    elif CONF_HEARTBEAT in config:
        yield App.register_component(HeartbeatFilter.new(config[CONF_HEARTBEAT]))
    elif CONF_DEBOUNCE in config:
        yield App.register_component(DebounceFilter.new(config[CONF_DEBOUNCE]))
    elif CONF_UNIQUE in config:
        yield UniqueFilter.new()


def setup_filters(config):
    filters = []
    for conf in config:
        for filter in setup_filter(conf):
            yield None
        filters.append(filter)
    yield filters


def setup_sensor_core_(sensor_var, config):
    if CONF_INTERNAL in config:
        add(sensor_var.set_internal(config[CONF_INTERNAL]))
    if CONF_UNIT_OF_MEASUREMENT in config:
        add(sensor_var.set_unit_of_measurement(config[CONF_UNIT_OF_MEASUREMENT]))
    if CONF_ICON in config:
        add(sensor_var.set_icon(config[CONF_ICON]))
    if CONF_ACCURACY_DECIMALS in config:
        add(sensor_var.set_accuracy_decimals(config[CONF_ACCURACY_DECIMALS]))
    if CONF_FILTERS in config:
        for filters in setup_filters(config[CONF_FILTERS]):
            yield
        add(sensor_var.set_filters(filters))

    for conf in config.get(CONF_ON_VALUE, []):
        rhs = sensor_var.make_state_trigger()
        trigger = Pvariable(conf[CONF_TRIGGER_ID], rhs)
        automation.build_automation(trigger, float_, conf)
    for conf in config.get(CONF_ON_RAW_VALUE, []):
        rhs = sensor_var.make_raw_state_trigger()
        trigger = Pvariable(conf[CONF_TRIGGER_ID], rhs)
        automation.build_automation(trigger, float_, conf)
    for conf in config.get(CONF_ON_VALUE_RANGE, []):
        rhs = sensor_var.make_value_range_trigger()
        trigger = Pvariable(conf[CONF_TRIGGER_ID], rhs)
        add(App.register_component(trigger))
        if CONF_ABOVE in conf:
            for template_ in templatable(conf[CONF_ABOVE], float_, float_):
                yield
            add(trigger.set_min(template_))
        if CONF_BELOW in conf:
            for template_ in templatable(conf[CONF_BELOW], float_, float_):
                yield
            add(trigger.set_max(template_))
        automation.build_automation(trigger, float_, conf)

    mqtt_ = sensor_var.Pget_mqtt()
    if CONF_EXPIRE_AFTER in config:
        if config[CONF_EXPIRE_AFTER] is None:
            add(mqtt_.disable_expire_after())
        else:
            add(mqtt_.set_expire_after(config[CONF_EXPIRE_AFTER]))
    setup_mqtt_component(mqtt_, config)


def setup_sensor(sensor_obj, config):
    if not CORE.has_id(config[CONF_ID]):
        sensor_obj = Pvariable(config[CONF_ID], sensor_obj, has_side_effects=True)
    CORE.add_job(setup_sensor_core_, sensor_obj, config)


def register_sensor(var, config):
    sensor_var = Pvariable(config[CONF_ID], var, has_side_effects=True)
    add(App.register_sensor(sensor_var))
    CORE.add_job(setup_sensor_core_, sensor_var, config)


BUILD_FLAGS = '-DUSE_SENSOR'

CONF_SENSOR_IN_RANGE = 'sensor.in_range'
SENSOR_IN_RANGE_CONDITION_SCHEMA = vol.All({
    vol.Required(CONF_ID): cv.use_variable_id(Sensor),
    vol.Optional(CONF_ABOVE): cv.float_,
    vol.Optional(CONF_BELOW): cv.float_,
}, cv.has_at_least_one_key(CONF_ABOVE, CONF_BELOW))


@CONDITION_REGISTRY.register(CONF_SENSOR_IN_RANGE, SENSOR_IN_RANGE_CONDITION_SCHEMA)
def sensor_in_range_to_code(config, condition_id, arg_type, template_arg):
    for var in get_variable(config[CONF_ID]):
        yield None
    rhs = var.make_sensor_in_range_condition(template_arg)
    type = SensorInRangeCondition.template(arg_type)
    cond = Pvariable(condition_id, rhs, type=type)

    if CONF_ABOVE in config:
        add(cond.set_min(config[CONF_ABOVE]))
    if CONF_BELOW in config:
        add(cond.set_max(config[CONF_BELOW]))

    yield cond


def core_to_hass_config(data, config):
    ret = mqtt.build_hass_config(data, 'sensor', config, include_state=True, include_command=False)
    if ret is None:
        return None
    if CONF_UNIT_OF_MEASUREMENT in config:
        ret['unit_of_measurement'] = config[CONF_UNIT_OF_MEASUREMENT]
    if CONF_EXPIRE_AFTER in config:
        expire = config[CONF_EXPIRE_AFTER]
        if expire is not None:
            ret['expire_after'] = expire.total_seconds
    if CONF_ICON in config:
        ret['icon'] = config[CONF_ICON]
    return ret
