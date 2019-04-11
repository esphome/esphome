import voluptuous as vol

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, core
from esphome.automation import CONDITION_REGISTRY, Condition, maybe_simple_id
from esphome.const import CONF_DEVICE_CLASS, CONF_FILTERS, \
    CONF_ID, CONF_INTERNAL, CONF_INVALID_COOLDOWN, CONF_INVERTED, \
    CONF_MAX_LENGTH, CONF_MIN_LENGTH, CONF_ON_CLICK, \
    CONF_ON_DOUBLE_CLICK, CONF_ON_MULTI_CLICK, CONF_ON_PRESS, CONF_ON_RELEASE, CONF_ON_STATE, \
    CONF_STATE, CONF_TIMING, CONF_TRIGGER_ID, CONF_FOR, CONF_VALUE
from esphome.core import CORE, coroutine
from esphome.py_compat import string_types
from esphome.util import ServiceRegistry

DEVICE_CLASSES = [
    '', 'battery', 'cold', 'connectivity', 'door', 'garage_door', 'gas',
    'heat', 'light', 'lock', 'moisture', 'motion', 'moving', 'occupancy',
    'opening', 'plug', 'power', 'presence', 'problem', 'safety', 'smoke',
    'sound', 'vibration', 'window'
]

PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({

})

binary_sensor_ns = cg.esphome_ns.namespace('binary_sensor')
BinarySensor = binary_sensor_ns.class_('BinarySensor', cg.Nameable)
BinarySensorPtr = BinarySensor.operator('ptr')
# MQTTBinarySensorComponent = binary_sensor_ns.class_('MQTTBinarySensorComponent', mqtt.MQTTComponent)

# Triggers
PressTrigger = binary_sensor_ns.class_('PressTrigger', cg.Trigger.template())
ReleaseTrigger = binary_sensor_ns.class_('ReleaseTrigger', cg.Trigger.template())
ClickTrigger = binary_sensor_ns.class_('ClickTrigger', cg.Trigger.template())
DoubleClickTrigger = binary_sensor_ns.class_('DoubleClickTrigger', cg.Trigger.template())
MultiClickTrigger = binary_sensor_ns.class_('MultiClickTrigger', cg.Trigger.template(),
                                            cg.Component)
MultiClickTriggerEvent = binary_sensor_ns.struct('MultiClickTriggerEvent')
StateTrigger = binary_sensor_ns.class_('StateTrigger', cg.Trigger.template(bool))

# Condition
BinarySensorCondition = binary_sensor_ns.class_('BinarySensorCondition', Condition)

# Filters
Filter = binary_sensor_ns.class_('Filter')
DelayedOnFilter = binary_sensor_ns.class_('DelayedOnFilter', Filter, cg.Component)
DelayedOffFilter = binary_sensor_ns.class_('DelayedOffFilter', Filter, cg.Component)
InvertFilter = binary_sensor_ns.class_('InvertFilter', Filter)
LambdaFilter = binary_sensor_ns.class_('LambdaFilter', Filter)

FILTER_REGISTRY = ServiceRegistry()
validate_filters = cv.validate_registry('filter', FILTER_REGISTRY, [CONF_ID])


@FILTER_REGISTRY.register('invert',
                          cv.Schema({
                              cv.GenerateID(): cv.declare_variable_id(InvertFilter)
                          }))
def invert_filter_to_code(config):
    rhs = InvertFilter.new()
    var = cg.Pvariable(config[CONF_ID], rhs)
    yield var


@FILTER_REGISTRY.register('delayed_on',
                          cv.maybe_simple_value(cv.Schema({
                              cv.GenerateID(): cv.declare_variable_id(DelayedOnFilter),
                              cv.Required(CONF_VALUE): cv.positive_time_period_milliseconds,
                          }).extend(cv.COMPONENT_SCHEMA)))
def delayed_on_filter_to_code(config):
    rhs = DelayedOnFilter.new(config[CONF_VALUE])
    var = cg.Pvariable(config[CONF_ID], rhs)
    yield cg.register_component(var, config)
    yield var


@FILTER_REGISTRY.register('delayed_off',
                          cv.maybe_simple_value(cv.Schema({
                              cv.GenerateID(): cv.declare_variable_id(DelayedOffFilter),
                              cv.Required(CONF_VALUE): cv.positive_time_period_milliseconds,
                          }).extend(cv.COMPONENT_SCHEMA)))
def delayed_off_filter_to_code(config):
    rhs = DelayedOffFilter.new(config[CONF_VALUE])
    var = cg.Pvariable(config[CONF_ID], rhs)
    yield cg.register_component(var, config)
    yield var


@FILTER_REGISTRY.register('lambda',
                          cv.maybe_simple_value(cv.Schema({
                              cv.GenerateID(): cv.declare_variable_id(LambdaFilter),
                              cv.Required(CONF_VALUE): cv.lambda_,
                          })))
def lambda_filter_to_code(config):
    lambda_ = yield cg.process_lambda(config[CONF_VALUE], [(bool, 'x')],
                                      return_type=cg.optional.template(bool))
    rhs = LambdaFilter.new(lambda_)
    var = cg.Pvariable(config[CONF_ID], rhs)
    yield var


MULTI_CLICK_TIMING_SCHEMA = cv.Schema({
    vol.Optional(CONF_STATE): cv.boolean,
    vol.Optional(CONF_MIN_LENGTH): cv.positive_time_period_milliseconds,
    vol.Optional(CONF_MAX_LENGTH): cv.positive_time_period_milliseconds,
})


def parse_multi_click_timing_str(value):
    if not isinstance(value, string_types):
        return value

    parts = value.lower().split(' ')
    if len(parts) != 5:
        raise vol.Invalid("Multi click timing grammar consists of exactly 5 words, not {}"
                          "".format(len(parts)))
    try:
        state = cv.boolean(parts[0])
    except vol.Invalid:
        raise vol.Invalid(u"First word must either be ON or OFF, not {}".format(parts[0]))

    if parts[1] != 'for':
        raise vol.Invalid(u"Second word must be 'for', got {}".format(parts[1]))

    if parts[2] == 'at':
        if parts[3] == 'least':
            key = CONF_MIN_LENGTH
        elif parts[3] == 'most':
            key = CONF_MAX_LENGTH
        else:
            raise vol.Invalid(u"Third word after at must either be 'least' or 'most', got {}"
                              u"".format(parts[3]))
        try:
            length = cv.positive_time_period_milliseconds(parts[4])
        except vol.Invalid as err:
            raise vol.Invalid(u"Multi Click Grammar Parsing length failed: {}".format(err))
        return {
            CONF_STATE: state,
            key: str(length)
        }

    if parts[3] != 'to':
        raise vol.Invalid("Multi click grammar: 4th word must be 'to'")

    try:
        min_length = cv.positive_time_period_milliseconds(parts[2])
    except vol.Invalid as err:
        raise vol.Invalid(u"Multi Click Grammar Parsing minimum length failed: {}".format(err))

    try:
        max_length = cv.positive_time_period_milliseconds(parts[4])
    except vol.Invalid as err:
        raise vol.Invalid(u"Multi Click Grammar Parsing minimum length failed: {}".format(err))

    return {
        CONF_STATE: state,
        CONF_MIN_LENGTH: str(min_length),
        CONF_MAX_LENGTH: str(max_length)
    }


def validate_multi_click_timing(value):
    if not isinstance(value, list):
        raise vol.Invalid("Timing option must be a *list* of times!")
    timings = []
    state = None
    for i, v_ in enumerate(value):
        v_ = MULTI_CLICK_TIMING_SCHEMA(v_)
        min_length = v_.get(CONF_MIN_LENGTH)
        max_length = v_.get(CONF_MAX_LENGTH)
        if min_length is None and max_length is None:
            raise vol.Invalid("At least one of min_length and max_length is required!")
        if min_length is None and max_length is not None:
            min_length = core.TimePeriodMilliseconds(milliseconds=0)

        new_state = v_.get(CONF_STATE, not state)
        if new_state == state:
            raise vol.Invalid("Timings must have alternating state. Indices {} and {} have "
                              "the same state {}".format(i, i + 1, state))
        if max_length is not None and max_length < min_length:
            raise vol.Invalid("Max length ({}) must be larger than min length ({})."
                              "".format(max_length, min_length))

        state = new_state
        tim = {
            CONF_STATE: new_state,
            CONF_MIN_LENGTH: min_length,
        }
        if max_length is not None:
            tim[CONF_MAX_LENGTH] = max_length
        timings.append(tim)
    return timings


device_class = cv.one_of(*DEVICE_CLASSES, lower=True, space='_')

BINARY_SENSOR_SCHEMA = cv.MQTT_COMPONENT_SCHEMA.extend({
    # cv.GenerateID(CONF_MQTT_ID): cv.declare_variable_id(MQTTBinarySensorComponent),

    vol.Optional(CONF_DEVICE_CLASS): device_class,
    vol.Optional(CONF_FILTERS): validate_filters,
    vol.Optional(CONF_ON_PRESS): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_variable_id(PressTrigger),
    }),
    vol.Optional(CONF_ON_RELEASE): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_variable_id(ReleaseTrigger),
    }),
    vol.Optional(CONF_ON_CLICK): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_variable_id(ClickTrigger),
        vol.Optional(CONF_MIN_LENGTH, default='50ms'): cv.positive_time_period_milliseconds,
        vol.Optional(CONF_MAX_LENGTH, default='350ms'): cv.positive_time_period_milliseconds,
    }),
    vol.Optional(CONF_ON_DOUBLE_CLICK): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_variable_id(DoubleClickTrigger),
        vol.Optional(CONF_MIN_LENGTH, default='50ms'): cv.positive_time_period_milliseconds,
        vol.Optional(CONF_MAX_LENGTH, default='350ms'): cv.positive_time_period_milliseconds,
    }),
    vol.Optional(CONF_ON_MULTI_CLICK): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_variable_id(MultiClickTrigger),
        vol.Required(CONF_TIMING): vol.All([parse_multi_click_timing_str],
                                           validate_multi_click_timing),
        vol.Optional(CONF_INVALID_COOLDOWN, default='1s'): cv.positive_time_period_milliseconds,
    }),
    vol.Optional(CONF_ON_STATE): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_variable_id(StateTrigger),
    }),

    vol.Optional(CONF_INVERTED): cv.invalid(
        "The inverted binary_sensor property has been replaced by the "
        "new 'invert' binary  sensor filter. Please see "
        "https://esphome.io/components/binary_sensor/index.html."
    ),
})

BINARY_SENSOR_PLATFORM_SCHEMA = PLATFORM_SCHEMA.extend(BINARY_SENSOR_SCHEMA.schema)


@coroutine
def setup_binary_sensor_core_(var, config):
    if CONF_INTERNAL in config:
        cg.add(var.set_internal(CONF_INTERNAL))
    if CONF_DEVICE_CLASS in config:
        cg.add(var.set_device_class(config[CONF_DEVICE_CLASS]))
    if CONF_INVERTED in config:
        cg.add(var.set_inverted(config[CONF_INVERTED]))
    if CONF_FILTERS in config:
        filters = yield cg.build_registry_list(FILTER_REGISTRY, config[CONF_FILTERS])
        cg.add(var.add_filters(filters))

    for conf in config.get(CONF_ON_PRESS, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        yield automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_RELEASE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        yield automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_CLICK, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var,
                                   conf[CONF_MIN_LENGTH], conf[CONF_MAX_LENGTH])
        yield automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_DOUBLE_CLICK, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var,
                                   conf[CONF_MIN_LENGTH], conf[CONF_MAX_LENGTH])
        yield automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_MULTI_CLICK, []):
        timings = []
        for tim in conf[CONF_TIMING]:
            timings.append(cg.StructInitializer(
                MultiClickTriggerEvent,
                ('state', tim[CONF_STATE]),
                ('min_length', tim[CONF_MIN_LENGTH]),
                ('max_length', tim.get(CONF_MAX_LENGTH, 4294967294)),
            ))
        trigger = cg.new_Pvariable(config[CONF_TRIGGER_ID], var, timings)
        if CONF_INVALID_COOLDOWN in conf:
            cg.add(trigger.set_invalid_cooldown(conf[CONF_INVALID_COOLDOWN]))
        yield automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_STATE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        yield automation.build_automation(trigger, [(bool, 'x')], conf)

    # setup_mqtt_component(binary_sensor_var.Pget_mqtt(), config)


@coroutine
def register_binary_sensor(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.register_binary_sensor(var))
    yield setup_binary_sensor_core_(var, config)


BINARY_SENSOR_IS_ON_OFF_CONDITION_SCHEMA = maybe_simple_id({
    vol.Required(CONF_ID): cv.use_variable_id(BinarySensor),
    vol.Optional(CONF_FOR): cv.positive_time_period_milliseconds,
})


@CONDITION_REGISTRY.register('binary_sensor.is_on', BINARY_SENSOR_IS_ON_OFF_CONDITION_SCHEMA)
def binary_sensor_is_on_to_code(config, condition_id, template_arg, args):
    var = yield cg.get_variable(config[CONF_ID])
    type = BinarySensorCondition.template(template_arg)
    rhs = type.new(var, True, config.get(CONF_FOR))
    yield cg.Pvariable(condition_id, rhs, type=type)


@CONDITION_REGISTRY.register('binary_sensor.is_off', BINARY_SENSOR_IS_ON_OFF_CONDITION_SCHEMA)
def binary_sensor_is_off_to_code(config, condition_id, template_arg, args):
    var = yield cg.get_variable(config[CONF_ID])
    type = BinarySensorCondition.template(template_arg)
    rhs = type.new(var, False, config.get(CONF_FOR))
    yield cg.Pvariable(condition_id, rhs, type=type)


def to_code(config):
    cg.add_define('USE_BINARY_SENSOR')
