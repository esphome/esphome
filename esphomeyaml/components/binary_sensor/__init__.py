import voluptuous as vol

from esphomeyaml import automation, core
from esphomeyaml.components import mqtt
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_DELAYED_OFF, CONF_DELAYED_ON, CONF_DEVICE_CLASS, CONF_FILTERS, \
    CONF_HEARTBEAT, CONF_ID, CONF_INTERNAL, CONF_INVALID_COOLDOWN, CONF_INVERT, CONF_INVERTED, \
    CONF_LAMBDA, CONF_MAX_LENGTH, CONF_MIN_LENGTH, CONF_MQTT_ID, CONF_ON_CLICK, \
    CONF_ON_DOUBLE_CLICK, CONF_ON_MULTI_CLICK, CONF_ON_PRESS, CONF_ON_RELEASE, CONF_STATE, \
    CONF_TIMING, CONF_TRIGGER_ID
from esphomeyaml.helpers import App, ArrayInitializer, NoArg, Pvariable, StructInitializer, add, \
    add_job, bool_, esphomelib_ns, process_lambda, setup_mqtt_component, Nameable, Trigger, \
    Component

DEVICE_CLASSES = [
    '', 'battery', 'cold', 'connectivity', 'door', 'garage_door', 'gas',
    'heat', 'light', 'lock', 'moisture', 'motion', 'moving', 'occupancy',
    'opening', 'plug', 'power', 'presence', 'problem', 'safety', 'smoke',
    'sound', 'vibration', 'window'
]

PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({

})

binary_sensor_ns = esphomelib_ns.namespace('binary_sensor')
BinarySensor = binary_sensor_ns.class_('BinarySensor', Nameable)
MQTTBinarySensorComponent = binary_sensor_ns.class_('MQTTBinarySensorComponent', mqtt.MQTTComponent)

# Triggers
PressTrigger = binary_sensor_ns.class_('PressTrigger', Trigger.template(NoArg))
ReleaseTrigger = binary_sensor_ns.class_('ReleaseTrigger', Trigger.template(NoArg))
ClickTrigger = binary_sensor_ns.class_('ClickTrigger', Trigger.template(NoArg))
DoubleClickTrigger = binary_sensor_ns.class_('DoubleClickTrigger', Trigger.template(NoArg))
MultiClickTrigger = binary_sensor_ns.class_('MultiClickTrigger', Trigger.template(NoArg), Component)
MultiClickTriggerEvent = binary_sensor_ns.struct('MultiClickTriggerEvent')

# Filters
Filter = binary_sensor_ns.class_('Filter')
DelayedOnFilter = binary_sensor_ns.class_('DelayedOnFilter', Filter, Component)
DelayedOffFilter = binary_sensor_ns.class_('DelayedOffFilter', Filter, Component)
HeartbeatFilter = binary_sensor_ns.class_('HeartbeatFilter', Filter, Component)
InvertFilter = binary_sensor_ns.class_('InvertFilter', Filter)
LambdaFilter = binary_sensor_ns.class_('LambdaFilter', Filter)


FILTER_KEYS = [CONF_INVERT, CONF_DELAYED_ON, CONF_DELAYED_OFF, CONF_LAMBDA, CONF_HEARTBEAT]

FILTERS_SCHEMA = vol.All(cv.ensure_list, [vol.All({
    vol.Optional(CONF_INVERT): None,
    vol.Optional(CONF_DELAYED_ON): cv.positive_time_period_milliseconds,
    vol.Optional(CONF_DELAYED_OFF): cv.positive_time_period_milliseconds,
    vol.Optional(CONF_HEARTBEAT): cv.positive_time_period_milliseconds,
    vol.Optional(CONF_LAMBDA): cv.lambda_,
}, cv.has_exactly_one_key(*FILTER_KEYS))])

MULTI_CLICK_TIMING_SCHEMA = vol.Schema({
    vol.Optional(CONF_STATE): cv.boolean,
    vol.Optional(CONF_MIN_LENGTH): cv.positive_time_period_milliseconds,
    vol.Optional(CONF_MAX_LENGTH): cv.positive_time_period_milliseconds,
})


def parse_multi_click_timing_str(value):
    if not isinstance(value, basestring):
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


BINARY_SENSOR_SCHEMA = cv.MQTT_COMPONENT_SCHEMA.extend({
    cv.GenerateID(CONF_MQTT_ID): cv.declare_variable_id(MQTTBinarySensorComponent),

    vol.Optional(CONF_DEVICE_CLASS): vol.All(vol.Lower, cv.one_of(*DEVICE_CLASSES)),
    vol.Optional(CONF_FILTERS): FILTERS_SCHEMA,
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
        vol.Optional(CONF_INVALID_COOLDOWN): cv.positive_time_period_milliseconds,
    }),

    vol.Optional(CONF_INVERTED): cv.invalid(
        "The inverted binary_sensor property has been replaced by the "
        "new 'invert' binary  sensor filter. Please see "
        "https://esphomelib.com/esphomeyaml/components/binary_sensor/index.html."
    ),
})

BINARY_SENSOR_PLATFORM_SCHEMA = PLATFORM_SCHEMA.extend(BINARY_SENSOR_SCHEMA.schema)


def setup_filter(config):
    if CONF_INVERT in config:
        yield InvertFilter.new()
    elif CONF_DELAYED_OFF in config:
        yield App.register_component(DelayedOffFilter.new(config[CONF_DELAYED_OFF]))
    elif CONF_DELAYED_ON in config:
        yield App.register_component(DelayedOnFilter.new(config[CONF_DELAYED_ON]))
    elif CONF_HEARTBEAT in config:
        yield App.register_component(HeartbeatFilter.new(config[CONF_HEARTBEAT]))
    elif CONF_LAMBDA in config:
        lambda_ = None
        for lambda_ in process_lambda(config[CONF_LAMBDA], [(bool_, 'x')]):
            yield None
        yield LambdaFilter.new(lambda_)


def setup_filters(config):
    filters = []
    for conf in config:
        filter = None
        for filter in setup_filter(conf):
            yield None
        filters.append(filter)
    yield ArrayInitializer(*filters)


def setup_binary_sensor_core_(binary_sensor_var, mqtt_var, config):
    if CONF_INTERNAL in config:
        add(binary_sensor_var.set_internal(CONF_INTERNAL))
    if CONF_DEVICE_CLASS in config:
        add(binary_sensor_var.set_device_class(config[CONF_DEVICE_CLASS]))
    if CONF_INVERTED in config:
        add(binary_sensor_var.set_inverted(config[CONF_INVERTED]))
    if CONF_FILTERS in config:
        filters = None
        for filters in setup_filters(config[CONF_FILTERS]):
            yield
        add(binary_sensor_var.add_filters(filters))

    for conf in config.get(CONF_ON_PRESS, []):
        rhs = binary_sensor_var.make_press_trigger()
        trigger = Pvariable(conf[CONF_TRIGGER_ID], rhs)
        automation.build_automation(trigger, NoArg, conf)

    for conf in config.get(CONF_ON_RELEASE, []):
        rhs = binary_sensor_var.make_release_trigger()
        trigger = Pvariable(conf[CONF_TRIGGER_ID], rhs)
        automation.build_automation(trigger, NoArg, conf)

    for conf in config.get(CONF_ON_CLICK, []):
        rhs = binary_sensor_var.make_click_trigger(conf[CONF_MIN_LENGTH], conf[CONF_MAX_LENGTH])
        trigger = Pvariable(conf[CONF_TRIGGER_ID], rhs)
        automation.build_automation(trigger, NoArg, conf)

    for conf in config.get(CONF_ON_DOUBLE_CLICK, []):
        rhs = binary_sensor_var.make_double_click_trigger(conf[CONF_MIN_LENGTH],
                                                          conf[CONF_MAX_LENGTH])
        trigger = Pvariable(conf[CONF_TRIGGER_ID], rhs)
        automation.build_automation(trigger, NoArg, conf)

    for conf in config.get(CONF_ON_MULTI_CLICK, []):
        timings = []
        for tim in conf[CONF_TIMING]:
            timings.append(StructInitializer(
                MultiClickTriggerEvent,
                ('state', tim[CONF_STATE]),
                ('min_length', tim[CONF_MIN_LENGTH]),
                ('max_length', tim.get(CONF_MAX_LENGTH, 4294967294)),
            ))
        timings = ArrayInitializer(*timings, multiline=False)
        rhs = App.register_component(binary_sensor_var.make_multi_click_trigger(timings))
        trigger = Pvariable(conf[CONF_TRIGGER_ID], rhs)
        if CONF_INVALID_COOLDOWN in conf:
            add(trigger.set_invalid_cooldown(conf[CONF_INVALID_COOLDOWN]))
        automation.build_automation(trigger, NoArg, conf)

    setup_mqtt_component(mqtt_var, config)


def setup_binary_sensor(binary_sensor_obj, mqtt_obj, config):
    binary_sensor_var = Pvariable(config[CONF_ID], binary_sensor_obj,
                                  has_side_effects=False)
    mqtt_var = Pvariable(config[CONF_MQTT_ID], mqtt_obj,
                         has_side_effects=False)
    add_job(setup_binary_sensor_core_, binary_sensor_var, mqtt_var, config)


def register_binary_sensor(var, config):
    binary_sensor_var = Pvariable(config[CONF_ID], var, has_side_effects=True)
    rhs = App.register_binary_sensor(binary_sensor_var)
    mqtt_var = Pvariable(config[CONF_MQTT_ID], rhs, has_side_effects=True)
    add_job(setup_binary_sensor_core_, binary_sensor_var, mqtt_var, config)


def core_to_hass_config(data, config):
    ret = mqtt.build_hass_config(data, 'binary_sensor', config,
                                 include_state=True, include_command=False)
    if ret is None:
        return None
    if CONF_DEVICE_CLASS in config:
        ret['device_class'] = config[CONF_DEVICE_CLASS]
    return ret


BUILD_FLAGS = '-DUSE_BINARY_SENSOR'
