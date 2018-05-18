import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ACCURACY_DECIMALS, CONF_ALPHA, CONF_DEBOUNCE, CONF_DELTA, \
    CONF_EXPIRE_AFTER, CONF_EXPONENTIAL_MOVING_AVERAGE, CONF_FILTERS, CONF_FILTER_NAN, \
    CONF_FILTER_OUT, CONF_HEARTBEAT, CONF_ICON, CONF_LAMBDA, CONF_MQTT_ID, CONF_MULTIPLY, \
    CONF_NAME, CONF_OFFSET, CONF_OR, CONF_SEND_EVERY, CONF_SLIDING_WINDOW_MOVING_AVERAGE, \
    CONF_THROTTLE, CONF_UNIQUE, CONF_UNIT_OF_MEASUREMENT, CONF_WINDOW_SIZE
from esphomeyaml.helpers import App, ArrayInitializer, MockObj, Pvariable, RawExpression, add, \
    setup_mqtt_component

PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({

})


def validate_recursive_filter(value):
    return FILTERS_SCHEMA(value)


FILTERS_SCHEMA = vol.All(cv.ensure_list, [vol.Any(
    vol.Schema({vol.Required(CONF_OFFSET): vol.Coerce(float)}),
    vol.Schema({vol.Required(CONF_MULTIPLY): vol.Coerce(float)}),
    vol.Schema({vol.Required(CONF_FILTER_OUT): vol.Coerce(float)}),
    vol.Schema({vol.Required(CONF_FILTER_NAN): None}),
    vol.Schema({
        vol.Required(CONF_SLIDING_WINDOW_MOVING_AVERAGE): vol.Schema({
            vol.Required(CONF_WINDOW_SIZE): cv.positive_not_null_int,
            vol.Required(CONF_SEND_EVERY): cv.positive_not_null_int,
        })
    }),
    vol.Schema({
        vol.Required(CONF_EXPONENTIAL_MOVING_AVERAGE): vol.Schema({
            vol.Required(CONF_ALPHA): cv.positive_float,
            vol.Required(CONF_SEND_EVERY): cv.positive_not_null_int,
        })
    }),
    vol.Schema({vol.Required(CONF_LAMBDA): cv.string_strict}),
    vol.Schema({vol.Required(CONF_THROTTLE): cv.positive_time_period_milliseconds}),
    vol.Schema({vol.Required(CONF_DELTA): vol.Coerce(float)}),
    vol.Schema({vol.Required(CONF_UNIQUE): None}),
    vol.Schema({vol.Required(CONF_HEARTBEAT): cv.positive_time_period_milliseconds}),
    vol.Schema({vol.Required(CONF_DEBOUNCE): cv.positive_time_period_milliseconds}),
    vol.Schema({vol.Required(CONF_OR): validate_recursive_filter}),
)])

MQTT_SENSOR_SCHEMA = vol.Schema({
    vol.Required(CONF_NAME): cv.string,
    vol.Optional(CONF_UNIT_OF_MEASUREMENT): cv.string_strict,
    vol.Optional(CONF_ICON): cv.icon,
    vol.Optional(CONF_ACCURACY_DECIMALS): vol.Coerce(int),
    vol.Optional(CONF_EXPIRE_AFTER): vol.Any(None, cv.positive_time_period_milliseconds),
    vol.Optional(CONF_FILTERS): FILTERS_SCHEMA
})

MQTT_SENSOR_ID_SCHEMA = MQTT_SENSOR_SCHEMA.extend({
    cv.GenerateID('mqtt_sensor', CONF_MQTT_ID): cv.register_variable_id,
})

# pylint: disable=invalid-name
OffsetFilter = MockObj('new sensor::OffsetFilter')
MultiplyFilter = MockObj('new sensor::MultiplyFilter')
FilterOutValueFilter = MockObj('new sensor::FilterOutValueFilter')
FilterOutNANFilter = MockObj('new sensor::FilterOutNANFilter')
SlidingWindowMovingAverageFilter = MockObj('new sensor::SlidingWindowMovingAverageFilter')
ExponentialMovingAverageFilter = MockObj('new sensor::ExponentialMovingAverageFilter')
LambdaFilter = MockObj('new sensor::LambdaFilter')
ThrottleFilter = MockObj('new sensor::ThrottleFilter')
DeltaFilter = MockObj('new sensor::DeltaFilter')
OrFilter = MockObj('new sensor::OrFilter')
HeartbeatFilter = MockObj('new sensor::HeartbeatFilter')
DebounceFilter = MockObj('new sensor::DebounceFilter')
UniqueFilter = MockObj('new sensor::UniqueFilter')


def setup_filter(config):
    if CONF_OFFSET in config:
        return OffsetFilter(config[CONF_OFFSET])
    if CONF_MULTIPLY in config:
        return MultiplyFilter(config[CONF_MULTIPLY])
    if CONF_FILTER_OUT in config:
        return FilterOutValueFilter(config[CONF_FILTER_OUT])
    if CONF_FILTER_NAN in config:
        return FilterOutNANFilter()
    if CONF_SLIDING_WINDOW_MOVING_AVERAGE in config:
        conf = config[CONF_SLIDING_WINDOW_MOVING_AVERAGE]
        return SlidingWindowMovingAverageFilter(conf[CONF_WINDOW_SIZE], conf[CONF_SEND_EVERY])
    if CONF_EXPONENTIAL_MOVING_AVERAGE in config:
        conf = config[CONF_EXPONENTIAL_MOVING_AVERAGE]
        return ExponentialMovingAverageFilter(conf[CONF_ALPHA], conf[CONF_SEND_EVERY])
    if CONF_LAMBDA in config:
        s = u'[](float x) -> Optional<float> {{ return {}; }}'.format(config[CONF_LAMBDA])
        return LambdaFilter(RawExpression(s))
    if CONF_THROTTLE in config:
        return ThrottleFilter(config[CONF_THROTTLE])
    if CONF_DELTA in config:
        return DeltaFilter(config[CONF_DELTA])
    if CONF_OR in config:
        return OrFilter(setup_filters(config[CONF_OR]))
    if CONF_HEARTBEAT in config:
        return App.register_component(HeartbeatFilter(config[CONF_HEARTBEAT]))
    if CONF_DEBOUNCE in config:
        return App.register_component(DebounceFilter(config[CONF_DEBOUNCE]))
    if CONF_UNIQUE in config:
        return UniqueFilter()
    raise ValueError(u"Filter unsupported: {}".format(config))


def setup_filters(config):
    return ArrayInitializer(*[setup_filter(x) for x in config])


def setup_mqtt_sensor_component(obj, config):
    if CONF_EXPIRE_AFTER in config:
        if config[CONF_EXPIRE_AFTER] is None:
            add(obj.disable_expire_after())
        else:
            add(obj.set_expire_after(config[CONF_EXPIRE_AFTER]))
    setup_mqtt_component(obj, config)


def setup_sensor(obj, config):
    if CONF_UNIT_OF_MEASUREMENT in config:
        add(obj.set_unit_of_measurement(config[CONF_UNIT_OF_MEASUREMENT]))
    if CONF_ICON in config:
        add(obj.set_icon(config[CONF_ICON]))
    if CONF_ACCURACY_DECIMALS in config:
        add(obj.set_accuracy_decimals(config[CONF_ACCURACY_DECIMALS]))
    if CONF_FILTERS in config:
        add(obj.set_filters(setup_filters(config[CONF_FILTERS])))


def register_sensor(var, config):
    setup_sensor(var, config)
    rhs = App.register_sensor(var)
    mqtt_sensor = Pvariable('sensor::MQTTSensorComponent', config[CONF_MQTT_ID], rhs)
    setup_mqtt_sensor_component(mqtt_sensor, config)


BUILD_FLAGS = '-DUSE_SENSOR'
