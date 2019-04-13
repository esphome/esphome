import math

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.automation import CONDITION_REGISTRY
from esphome.const import CONF_ABOVE, CONF_ACCURACY_DECIMALS, CONF_ALPHA, CONF_BELOW, \
    CONF_CALIBRATE_LINEAR, CONF_DEBOUNCE, CONF_DELTA, CONF_EXPIRE_AFTER, \
    CONF_FILTERS, CONF_FROM, \
    CONF_ICON, CONF_ID, CONF_INTERNAL, CONF_LAMBDA, CONF_MULTIPLY, CONF_OFFSET, \
    CONF_ON_RAW_VALUE, CONF_ON_VALUE, CONF_ON_VALUE_RANGE, CONF_OR, \
    CONF_SEND_EVERY, CONF_SEND_FIRST_AT, CONF_THROTTLE, CONF_TO, CONF_TRIGGER_ID, \
    CONF_UNIT_OF_MEASUREMENT, \
    CONF_WINDOW_SIZE, CONF_VALUE, CONF_HEARTBEAT, CONF_NAME
from esphome.core import CORE, coroutine
from esphome.util import ServiceRegistry

PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({

})


def validate_send_first_at(value):
    send_first_at = value.get(CONF_SEND_FIRST_AT)
    send_every = value[CONF_SEND_EVERY]
    if send_first_at is not None and send_first_at > send_every:
        raise cv.Invalid("send_first_at must be smaller than or equal to send_every! {} <= {}"
                         "".format(send_first_at, send_every))
    return value


FILTER_REGISTRY = ServiceRegistry()
validate_filters = cv.validate_registry('filter', FILTER_REGISTRY, [CONF_ID])


def validate_datapoint(value):
    if isinstance(value, dict):
        return cv.Schema({
            cv.Required(CONF_FROM): cv.float_,
            cv.Required(CONF_TO): cv.float_,
        })(value)
    value = cv.string(value)
    if '->' not in value:
        raise cv.Invalid("Datapoint mapping must contain '->'")
    a, b = value.split('->', 1)
    a, b = a.strip(), b.strip()
    return validate_datapoint({
        CONF_FROM: cv.float_(a),
        CONF_TO: cv.float_(b)
    })


# Base
sensor_ns = cg.esphome_ns.namespace('sensor')
Sensor = sensor_ns.class_('Sensor', cg.Nameable)
SensorPtr = Sensor.operator('ptr')

PollingSensorComponent = sensor_ns.class_('PollingSensorComponent', cg.PollingComponent, Sensor)

# Triggers
SensorStateTrigger = sensor_ns.class_('SensorStateTrigger', cg.Trigger.template(cg.float_))
SensorRawStateTrigger = sensor_ns.class_('SensorRawStateTrigger', cg.Trigger.template(cg.float_))
ValueRangeTrigger = sensor_ns.class_('ValueRangeTrigger', cg.Trigger.template(cg.float_),
                                     cg.Component)
SensorPublishAction = sensor_ns.class_('SensorPublishAction', cg.Action)

# Filters
Filter = sensor_ns.class_('Filter')
SlidingWindowMovingAverageFilter = sensor_ns.class_('SlidingWindowMovingAverageFilter', Filter)
ExponentialMovingAverageFilter = sensor_ns.class_('ExponentialMovingAverageFilter', Filter)
LambdaFilter = sensor_ns.class_('LambdaFilter', Filter)
OffsetFilter = sensor_ns.class_('OffsetFilter', Filter)
MultiplyFilter = sensor_ns.class_('MultiplyFilter', Filter)
FilterOutValueFilter = sensor_ns.class_('FilterOutValueFilter', Filter)
ThrottleFilter = sensor_ns.class_('ThrottleFilter', Filter)
DebounceFilter = sensor_ns.class_('DebounceFilter', Filter, cg.Component)
HeartbeatFilter = sensor_ns.class_('HeartbeatFilter', Filter, cg.Component)
DeltaFilter = sensor_ns.class_('DeltaFilter', Filter)
OrFilter = sensor_ns.class_('OrFilter', Filter)
CalibrateLinearFilter = sensor_ns.class_('CalibrateLinearFilter', Filter)
SensorInRangeCondition = sensor_ns.class_('SensorInRangeCondition', Filter)

unit_of_measurement = cv.string_strict
accuracy_decimals = cv.int_
icon = cv.icon

SENSOR_SCHEMA = cv.MQTT_COMPONENT_SCHEMA.extend({
    # cv.GenerateID(CONF_MQTT_ID): cv.declare_variable_id(MQTTSensorComponent),
    cv.GenerateID(): cv.declare_variable_id(Sensor),
    cv.Optional(CONF_UNIT_OF_MEASUREMENT): unit_of_measurement,
    cv.Optional(CONF_ICON): icon,
    cv.Optional(CONF_ACCURACY_DECIMALS): accuracy_decimals,
    cv.Optional(CONF_EXPIRE_AFTER): cv.All(cv.requires_component('mqtt'),
                                           cv.Any(None, cv.positive_time_period_milliseconds)),
    cv.Optional(CONF_FILTERS): validate_filters,
    cv.Optional(CONF_ON_VALUE): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_variable_id(SensorStateTrigger),
    }),
    cv.Optional(CONF_ON_RAW_VALUE): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_variable_id(SensorRawStateTrigger),
    }),
    cv.Optional(CONF_ON_VALUE_RANGE): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_variable_id(ValueRangeTrigger),
        cv.Optional(CONF_ABOVE): cv.float_,
        cv.Optional(CONF_BELOW): cv.float_,
    }, cv.has_at_least_one_key(CONF_ABOVE, CONF_BELOW)),
})

SENSOR_PLATFORM_SCHEMA = PLATFORM_SCHEMA.extend(SENSOR_SCHEMA.schema)


@FILTER_REGISTRY.register(CONF_OFFSET, cv.maybe_simple_value(cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(OffsetFilter),
    cv.Required(CONF_VALUE): cv.float_,
})))
def offset_filter_to_code(config):
    yield cg.new_Pvariable(config[CONF_ID], config[CONF_VALUE])


@FILTER_REGISTRY.register(CONF_MULTIPLY, cv.maybe_simple_value(cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(MultiplyFilter),
    cv.Required(CONF_VALUE): cv.float_,
})))
def multiply_filter_to_code(config):
    yield cg.new_Pvariable(config[CONF_ID], config[CONF_VALUE])


@FILTER_REGISTRY.register('filter_out', cv.maybe_simple_value(cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(FilterOutValueFilter),
    cv.Required(CONF_VALUE): cv.float_,
})))
def filter_out_filter_to_code(config):
    yield cg.new_Pvariable(config[CONF_ID], config[CONF_VALUE])


@FILTER_REGISTRY.register('sliding_window_moving_average', cv.All(cv.Schema({
    cv.GenerateID():
        cv.declare_variable_id(SlidingWindowMovingAverageFilter),
    cv.Optional(CONF_WINDOW_SIZE, default=15): cv.positive_not_null_int,
    cv.Optional(CONF_SEND_EVERY, default=15): cv.positive_not_null_int,
    cv.Optional(CONF_SEND_FIRST_AT, default=1): cv.positive_not_null_int,
}), validate_send_first_at))
def sliding_window_moving_average_filter_to_code(config):
    yield cg.new_Pvariable(config[CONF_ID], config[CONF_WINDOW_SIZE], config[CONF_SEND_EVERY],
                           config[CONF_SEND_FIRST_AT])


@FILTER_REGISTRY.register('exponential_moving_average', cv.Schema({
    cv.GenerateID():
        cv.declare_variable_id(ExponentialMovingAverageFilter),
    cv.Optional(CONF_ALPHA, default=0.1): cv.positive_float,
    cv.Optional(CONF_SEND_EVERY, default=15): cv.positive_not_null_int,
}))
def exponential_moving_average_filter_to_code(config):
    yield cg.new_Pvariable(config[CONF_ID], config[CONF_ALPHA], config[CONF_SEND_EVERY])


@FILTER_REGISTRY.register(CONF_LAMBDA, cv.maybe_simple_value(cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(LambdaFilter),
    cv.Required(CONF_VALUE): cv.lambda_,
})))
def lambda_filter_to_code(config):
    lambda_ = yield cg.process_lambda(config[CONF_LAMBDA], [(float, 'x')],
                                      return_type=cg.optional.template(float))
    yield cg.new_Pvariable(config[CONF_ID], lambda_)


@FILTER_REGISTRY.register(CONF_THROTTLE, cv.maybe_simple_value(cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(ThrottleFilter),
    cv.Required(CONF_VALUE): cv.positive_time_period_milliseconds,
})))
def throttle_filter_to_code(config):
    yield cg.new_Pvariable(config[CONF_ID], config[CONF_VALUE])


@FILTER_REGISTRY.register(CONF_DELTA, cv.maybe_simple_value(cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(DeltaFilter),
    cv.Required(CONF_VALUE): cv.float_,
})))
def delta_filter_to_code(config):
    yield cg.new_Pvariable(config[CONF_ID], config[CONF_VALUE])


@FILTER_REGISTRY.register(CONF_OR, cv.maybe_simple_value(cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(OrFilter),
    cv.Required(CONF_VALUE): validate_filters,
})))
def or_filter_to_code(config):
    filters = yield build_filters(config[CONF_VALUE])
    yield cg.new_Pvariable(config[CONF_ID], filters)


@FILTER_REGISTRY.register(CONF_THROTTLE, cv.maybe_simple_value(cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(ThrottleFilter),
    cv.Required(CONF_VALUE): cv.positive_time_period_milliseconds,
})))
def throttle_filter_to_code(config):
    yield cg.new_Pvariable(config[CONF_ID], config[CONF_VALUE])


@FILTER_REGISTRY.register(CONF_HEARTBEAT, cv.maybe_simple_value(cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(HeartbeatFilter),
    cv.Required(CONF_VALUE): cv.positive_time_period_milliseconds,
}).extend(cv.COMPONENT_SCHEMA)))
def heartbeat_filter_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_VALUE])
    yield cg.register_component(var, config)
    yield var


@FILTER_REGISTRY.register(CONF_DEBOUNCE, cv.maybe_simple_value(cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(DebounceFilter),
    cv.Required(CONF_VALUE): cv.positive_time_period_milliseconds,
}).extend(cv.COMPONENT_SCHEMA)))
def debounce_filter_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_VALUE])
    yield cg.register_component(var, config)
    yield var


@FILTER_REGISTRY.register(CONF_CALIBRATE_LINEAR, cv.maybe_simple_value(cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(CalibrateLinearFilter),
    cv.Required(CONF_VALUE): cv.All(
        cv.ensure_list(validate_datapoint), cv.Length(min=2)),
}).extend(cv.COMPONENT_SCHEMA)))
def calibrate_linear_filter_to_code(config):
    x = [conf[CONF_FROM] for conf in config]
    y = [conf[CONF_TO] for conf in config]
    k, b = fit_linear(x, y)
    yield cg.new_Pvariable(config[CONF_ID], k, b)


@coroutine
def build_filters(config):
    yield cg.build_registry_list(FILTER_REGISTRY, config)


@coroutine
def setup_sensor_core_(var, config):
    if CONF_INTERNAL in config:
        cg.add(var.set_internal(config[CONF_INTERNAL]))
    if CONF_UNIT_OF_MEASUREMENT in config:
        cg.add(var.set_unit_of_measurement(config[CONF_UNIT_OF_MEASUREMENT]))
    if CONF_ICON in config:
        cg.add(var.set_icon(config[CONF_ICON]))
    if CONF_ACCURACY_DECIMALS in config:
        cg.add(var.set_accuracy_decimals(config[CONF_ACCURACY_DECIMALS]))
    if CONF_FILTERS in config:
        filters = yield build_filters(config[CONF_FILTERS])
        cg.add(var.set_filters(filters))

    for conf in config.get(CONF_ON_VALUE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        yield automation.build_automation(trigger, [(float, 'x')], conf)
    for conf in config.get(CONF_ON_RAW_VALUE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        yield automation.build_automation(trigger, [(float, 'x')], conf)
    for conf in config.get(CONF_ON_VALUE_RANGE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        yield cg.register_component(trigger, conf)
        if CONF_ABOVE in conf:
            template_ = yield cg.templatable(conf[CONF_ABOVE], [(float, 'x')], float)
            cg.add(trigger.set_min(template_))
        if CONF_BELOW in conf:
            template_ = yield cg.templatable(conf[CONF_BELOW], [(float, 'x')], float)
            cg.add(trigger.set_max(template_))
        yield automation.build_automation(trigger, [(float, 'x')], conf)

    mqtt_ = var.Pget_mqtt()
    if CONF_EXPIRE_AFTER in config:
        if config[CONF_EXPIRE_AFTER] is None:
            cg.add(mqtt_.disable_expire_after())
        else:
            cg.add(mqtt_.set_expire_after(config[CONF_EXPIRE_AFTER]))
    # setup_mqtt_component(mqtt_, config)


@coroutine
def register_sensor(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.register_sensor(var))
    yield setup_sensor_core_(var, config)


@coroutine
def new_sensor(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME])
    yield register_sensor(var, config)
    yield var


CONF_SENSOR_IN_RANGE = 'sensor.in_range'
SENSOR_IN_RANGE_CONDITION_SCHEMA = cv.All({
    cv.Required(CONF_ID): cv.use_variable_id(Sensor),
    cv.Optional(CONF_ABOVE): cv.float_,
    cv.Optional(CONF_BELOW): cv.float_,
}, cv.has_at_least_one_key(CONF_ABOVE, CONF_BELOW))


@CONDITION_REGISTRY.register(CONF_SENSOR_IN_RANGE, SENSOR_IN_RANGE_CONDITION_SCHEMA)
def sensor_in_range_to_code(config, condition_id, template_arg, args):
    var = yield cg.get_variable(config[CONF_ID])
    rhs = var.make_sensor_in_range_condition(template_arg)
    type = SensorInRangeCondition.template(template_arg)
    cond = cg.Pvariable(condition_id, rhs, type=type)

    if CONF_ABOVE in config:
        cg.add(cond.set_min(config[CONF_ABOVE]))
    if CONF_BELOW in config:
        cg.add(cond.set_max(config[CONF_BELOW]))

    yield cond


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


def to_code(config):
    cg.add_define('USE_SENSOR')
    cg.add_global(sensor_ns.using)
