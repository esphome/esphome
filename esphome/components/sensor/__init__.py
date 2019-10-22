import math

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import mqtt
from esphome.const import CONF_ABOVE, CONF_ACCURACY_DECIMALS, CONF_ALPHA, CONF_BELOW, \
    CONF_EXPIRE_AFTER, CONF_FILTERS, CONF_FROM, CONF_ICON, CONF_ID, CONF_INTERNAL, \
    CONF_ON_RAW_VALUE, CONF_ON_VALUE, CONF_ON_VALUE_RANGE, CONF_SEND_EVERY, CONF_SEND_FIRST_AT, \
    CONF_TO, CONF_TRIGGER_ID, CONF_UNIT_OF_MEASUREMENT, CONF_WINDOW_SIZE, CONF_NAME, CONF_MQTT_ID, \
    CONF_FORCE_UPDATE
from esphome.core import CORE, coroutine, coroutine_with_priority
from esphome.util import Registry

IS_PLATFORM_COMPONENT = True


def validate_send_first_at(value):
    send_first_at = value.get(CONF_SEND_FIRST_AT)
    send_every = value[CONF_SEND_EVERY]
    if send_first_at is not None and send_first_at > send_every:
        raise cv.Invalid("send_first_at must be smaller than or equal to send_every! {} <= {}"
                         "".format(send_first_at, send_every))
    return value


FILTER_REGISTRY = Registry()
validate_filters = cv.validate_registry('filter', FILTER_REGISTRY)


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

# Triggers
SensorStateTrigger = sensor_ns.class_('SensorStateTrigger', automation.Trigger.template(cg.float_))
SensorRawStateTrigger = sensor_ns.class_('SensorRawStateTrigger',
                                         automation.Trigger.template(cg.float_))
ValueRangeTrigger = sensor_ns.class_('ValueRangeTrigger', automation.Trigger.template(cg.float_),
                                     cg.Component)
SensorPublishAction = sensor_ns.class_('SensorPublishAction', automation.Action)

# Filters
Filter = sensor_ns.class_('Filter')
MedianFilter = sensor_ns.class_('MedianFilter', Filter)
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
CalibratePolynomialFilter = sensor_ns.class_('CalibratePolynomialFilter', Filter)
SensorInRangeCondition = sensor_ns.class_('SensorInRangeCondition', Filter)

unit_of_measurement = cv.string_strict
accuracy_decimals = cv.int_
icon = cv.icon

SENSOR_SCHEMA = cv.MQTT_COMPONENT_SCHEMA.extend({
    cv.OnlyWith(CONF_MQTT_ID, 'mqtt'): cv.declare_id(mqtt.MQTTSensorComponent),
    cv.GenerateID(): cv.declare_id(Sensor),
    cv.Optional(CONF_UNIT_OF_MEASUREMENT): unit_of_measurement,
    cv.Optional(CONF_ICON): icon,
    cv.Optional(CONF_ACCURACY_DECIMALS): accuracy_decimals,
    cv.Optional(CONF_FORCE_UPDATE, default=False): cv.boolean,
    cv.Optional(CONF_EXPIRE_AFTER): cv.All(cv.requires_component('mqtt'),
                                           cv.Any(None, cv.positive_time_period_milliseconds)),
    cv.Optional(CONF_FILTERS): validate_filters,
    cv.Optional(CONF_ON_VALUE): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SensorStateTrigger),
    }),
    cv.Optional(CONF_ON_RAW_VALUE): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SensorRawStateTrigger),
    }),
    cv.Optional(CONF_ON_VALUE_RANGE): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ValueRangeTrigger),
        cv.Optional(CONF_ABOVE): cv.float_,
        cv.Optional(CONF_BELOW): cv.float_,
    }, cv.has_at_least_one_key(CONF_ABOVE, CONF_BELOW)),
})


def sensor_schema(unit_of_measurement_, icon_, accuracy_decimals_):
    # type: (str, str, int) -> cv.Schema
    return SENSOR_SCHEMA.extend({
        cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=unit_of_measurement_): unit_of_measurement,
        cv.Optional(CONF_ICON, default=icon_): icon,
        cv.Optional(CONF_ACCURACY_DECIMALS, default=accuracy_decimals_): accuracy_decimals,
    })


@FILTER_REGISTRY.register('offset', OffsetFilter, cv.float_)
def offset_filter_to_code(config, filter_id):
    yield cg.new_Pvariable(filter_id, config)


@FILTER_REGISTRY.register('multiply', MultiplyFilter, cv.float_)
def multiply_filter_to_code(config, filter_id):
    yield cg.new_Pvariable(filter_id, config)


@FILTER_REGISTRY.register('filter_out', FilterOutValueFilter, cv.float_)
def filter_out_filter_to_code(config, filter_id):
    yield cg.new_Pvariable(filter_id, config)


MEDIAN_SCHEMA = cv.All(cv.Schema({
    cv.Optional(CONF_WINDOW_SIZE, default=5): cv.positive_not_null_int,
    cv.Optional(CONF_SEND_EVERY, default=5): cv.positive_not_null_int,
    cv.Optional(CONF_SEND_FIRST_AT, default=1): cv.positive_not_null_int,
}), validate_send_first_at)


@FILTER_REGISTRY.register('median', MedianFilter, MEDIAN_SCHEMA)
def median_filter_to_code(config, filter_id):
    yield cg.new_Pvariable(filter_id, config[CONF_WINDOW_SIZE], config[CONF_SEND_EVERY],
                           config[CONF_SEND_FIRST_AT])


SLIDING_AVERAGE_SCHEMA = cv.All(cv.Schema({
    cv.Optional(CONF_WINDOW_SIZE, default=15): cv.positive_not_null_int,
    cv.Optional(CONF_SEND_EVERY, default=15): cv.positive_not_null_int,
    cv.Optional(CONF_SEND_FIRST_AT, default=1): cv.positive_not_null_int,
}), validate_send_first_at)


@FILTER_REGISTRY.register('sliding_window_moving_average', SlidingWindowMovingAverageFilter,
                          SLIDING_AVERAGE_SCHEMA)
def sliding_window_moving_average_filter_to_code(config, filter_id):
    yield cg.new_Pvariable(filter_id, config[CONF_WINDOW_SIZE], config[CONF_SEND_EVERY],
                           config[CONF_SEND_FIRST_AT])


@FILTER_REGISTRY.register('exponential_moving_average', ExponentialMovingAverageFilter, cv.Schema({
    cv.Optional(CONF_ALPHA, default=0.1): cv.positive_float,
    cv.Optional(CONF_SEND_EVERY, default=15): cv.positive_not_null_int,
}))
def exponential_moving_average_filter_to_code(config, filter_id):
    yield cg.new_Pvariable(filter_id, config[CONF_ALPHA], config[CONF_SEND_EVERY])


@FILTER_REGISTRY.register('lambda', LambdaFilter, cv.returning_lambda)
def lambda_filter_to_code(config, filter_id):
    lambda_ = yield cg.process_lambda(config, [(float, 'x')],
                                      return_type=cg.optional.template(float))
    yield cg.new_Pvariable(filter_id, lambda_)


@FILTER_REGISTRY.register('delta', DeltaFilter, cv.float_)
def delta_filter_to_code(config, filter_id):
    yield cg.new_Pvariable(filter_id, config)


@FILTER_REGISTRY.register('or', OrFilter, validate_filters)
def or_filter_to_code(config, filter_id):
    filters = yield build_filters(config)
    yield cg.new_Pvariable(filter_id, filters)


@FILTER_REGISTRY.register('throttle', ThrottleFilter, cv.positive_time_period_milliseconds)
def throttle_filter_to_code(config, filter_id):
    yield cg.new_Pvariable(filter_id, config)


@FILTER_REGISTRY.register('heartbeat', HeartbeatFilter, cv.positive_time_period_milliseconds)
def heartbeat_filter_to_code(config, filter_id):
    var = cg.new_Pvariable(filter_id, config)
    yield cg.register_component(var, {})
    yield var


@FILTER_REGISTRY.register('debounce', DebounceFilter, cv.positive_time_period_milliseconds)
def debounce_filter_to_code(config, filter_id):
    var = cg.new_Pvariable(filter_id, config)
    yield cg.register_component(var, {})
    yield var


def validate_not_all_from_same(config):
    if all(conf[CONF_FROM] == config[0][CONF_FROM] for conf in config):
        raise cv.Invalid("The 'from' values of the calibrate_linear filter cannot all point "
                         "to the same value! Please add more values to the filter.")
    return config


@FILTER_REGISTRY.register('calibrate_linear', CalibrateLinearFilter, cv.All(
    cv.ensure_list(validate_datapoint), cv.Length(min=2), validate_not_all_from_same))
def calibrate_linear_filter_to_code(config, filter_id):
    x = [conf[CONF_FROM] for conf in config]
    y = [conf[CONF_TO] for conf in config]
    k, b = fit_linear(x, y)
    yield cg.new_Pvariable(filter_id, k, b)


CONF_DATAPOINTS = 'datapoints'
CONF_DEGREE = 'degree'


def validate_calibrate_polynomial(config):
    if config[CONF_DEGREE] >= len(config[CONF_DATAPOINTS]):
        raise cv.Invalid("Degree is too high! Maximum possible degree with given datapoints is "
                         "{}".format(len(config[CONF_DATAPOINTS]) - 1), [CONF_DEGREE])
    return config


@FILTER_REGISTRY.register('calibrate_polynomial', CalibratePolynomialFilter, cv.All(cv.Schema({
    cv.Required(CONF_DATAPOINTS): cv.All(cv.ensure_list(validate_datapoint), cv.Length(min=1)),
    cv.Required(CONF_DEGREE): cv.positive_int,
}), validate_calibrate_polynomial))
def calibrate_polynomial_filter_to_code(config, filter_id):
    x = [conf[CONF_FROM] for conf in config[CONF_DATAPOINTS]]
    y = [conf[CONF_TO] for conf in config[CONF_DATAPOINTS]]
    degree = config[CONF_DEGREE]
    a = [[1] + [x_**(i+1) for i in range(degree)] for x_ in x]
    # Column vector
    b = [[v] for v in y]
    res = [v[0] for v in _lstsq(a, b)]
    yield cg.new_Pvariable(filter_id, res)


@coroutine
def build_filters(config):
    yield cg.build_registry_list(FILTER_REGISTRY, config)


@coroutine
def setup_sensor_core_(var, config):
    cg.add(var.set_name(config[CONF_NAME]))
    if CONF_INTERNAL in config:
        cg.add(var.set_internal(config[CONF_INTERNAL]))
    if CONF_UNIT_OF_MEASUREMENT in config:
        cg.add(var.set_unit_of_measurement(config[CONF_UNIT_OF_MEASUREMENT]))
    if CONF_ICON in config:
        cg.add(var.set_icon(config[CONF_ICON]))
    if CONF_ACCURACY_DECIMALS in config:
        cg.add(var.set_accuracy_decimals(config[CONF_ACCURACY_DECIMALS]))
    cg.add(var.set_force_update(config[CONF_FORCE_UPDATE]))
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

    if CONF_MQTT_ID in config:
        mqtt_ = cg.new_Pvariable(config[CONF_MQTT_ID], var)
        yield mqtt.register_mqtt_component(mqtt_, config)

        if CONF_EXPIRE_AFTER in config:
            if config[CONF_EXPIRE_AFTER] is None:
                cg.add(mqtt_.disable_expire_after())
            else:
                cg.add(mqtt_.set_expire_after(config[CONF_EXPIRE_AFTER]))


@coroutine
def register_sensor(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.register_sensor(var))
    yield setup_sensor_core_(var, config)


@coroutine
def new_sensor(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield register_sensor(var, config)
    yield var


SENSOR_IN_RANGE_CONDITION_SCHEMA = cv.All({
    cv.Required(CONF_ID): cv.use_id(Sensor),
    cv.Optional(CONF_ABOVE): cv.float_,
    cv.Optional(CONF_BELOW): cv.float_,
}, cv.has_at_least_one_key(CONF_ABOVE, CONF_BELOW))


@automation.register_condition('sensor.in_range', SensorInRangeCondition,
                               SENSOR_IN_RANGE_CONDITION_SCHEMA)
def sensor_in_range_to_code(config, condition_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(condition_id, template_arg, paren)

    if CONF_ABOVE in config:
        cg.add(var.set_min(config[CONF_ABOVE]))
    if CONF_BELOW in config:
        cg.add(var.set_max(config[CONF_BELOW]))

    yield var


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


def _mat_copy(m):
    return [list(row) for row in m]


def _mat_transpose(m):
    return _mat_copy(zip(*m))


def _mat_identity(n):
    return [[int(i == j) for j in range(n)] for i in range(n)]


def _mat_dot(a, b):
    b_t = _mat_transpose(b)
    return [[sum(x*y for x, y in zip(row_a, col_b)) for col_b in b_t] for row_a in a]


def _mat_inverse(m):
    n = len(m)
    m = _mat_copy(m)
    id = _mat_identity(n)

    for diag in range(n):
        # If diag element is 0, swap rows
        if m[diag][diag] == 0:
            for i in range(diag+1, n):
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
def to_code(config):
    cg.add_define('USE_SENSOR')
    cg.add_global(sensor_ns.using)
