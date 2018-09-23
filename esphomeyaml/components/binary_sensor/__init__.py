import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import automation
from esphomeyaml.const import CONF_DEVICE_CLASS, CONF_ID, CONF_INTERNAL, CONF_INVERTED, \
    CONF_MAX_LENGTH, CONF_MIN_LENGTH, CONF_MQTT_ID, CONF_ON_CLICK, CONF_ON_DOUBLE_CLICK, \
    CONF_ON_PRESS, CONF_ON_RELEASE, CONF_TRIGGER_ID, CONF_FILTERS, CONF_INVERT, CONF_DELAYED_ON, \
    CONF_DELAYED_OFF, CONF_LAMBDA, CONF_HEARTBEAT
from esphomeyaml.helpers import App, NoArg, Pvariable, add, add_job, esphomelib_ns, \
    setup_mqtt_component, bool_, process_lambda, ArrayInitializer

DEVICE_CLASSES = [
    '', 'battery', 'cold', 'connectivity', 'door', 'garage_door', 'gas',
    'heat', 'light', 'lock', 'moisture', 'motion', 'moving', 'occupancy',
    'opening', 'plug', 'power', 'presence', 'problem', 'safety', 'smoke',
    'sound', 'vibration', 'window'
]

PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({

})

binary_sensor_ns = esphomelib_ns.namespace('binary_sensor')
PressTrigger = binary_sensor_ns.PressTrigger
ReleaseTrigger = binary_sensor_ns.ReleaseTrigger
ClickTrigger = binary_sensor_ns.ClickTrigger
DoubleClickTrigger = binary_sensor_ns.DoubleClickTrigger
BinarySensor = binary_sensor_ns.BinarySensor
InvertFilter = binary_sensor_ns.InvertFilter
LambdaFilter = binary_sensor_ns.LambdaFilter
DelayedOnFilter = binary_sensor_ns.DelayedOnFilter
DelayedOffFilter = binary_sensor_ns.DelayedOffFilter
HeartbeatFilter = binary_sensor_ns.HeartbeatFilter
MQTTBinarySensorComponent = binary_sensor_ns.MQTTBinarySensorComponent

FILTER_KEYS = [CONF_INVERT, CONF_DELAYED_ON, CONF_DELAYED_OFF, CONF_LAMBDA]

FILTERS_SCHEMA = vol.All(cv.ensure_list, [vol.All({
    vol.Optional(CONF_INVERT): None,
    vol.Optional(CONF_DELAYED_ON): cv.positive_time_period_milliseconds,
    vol.Optional(CONF_DELAYED_OFF): cv.positive_time_period_milliseconds,
    vol.Optional(CONF_HEARTBEAT): cv.positive_time_period_milliseconds,
    vol.Optional(CONF_LAMBDA): cv.lambda_,
}, cv.has_exactly_one_key(*FILTER_KEYS))])

BINARY_SENSOR_SCHEMA = cv.MQTT_COMPONENT_SCHEMA.extend({
    cv.GenerateID(CONF_MQTT_ID): cv.declare_variable_id(MQTTBinarySensorComponent),
    cv.GenerateID(): cv.declare_variable_id(BinarySensor),

    vol.Optional(CONF_DEVICE_CLASS): vol.All(vol.Lower, cv.one_of(*DEVICE_CLASSES)),
    vol.Optional(CONF_FILTERS): FILTERS_SCHEMA,
    vol.Optional(CONF_ON_PRESS): vol.All(cv.ensure_list, [automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_variable_id(PressTrigger),
    })]),
    vol.Optional(CONF_ON_RELEASE): vol.All(cv.ensure_list, [automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_variable_id(ReleaseTrigger),
    })]),
    vol.Optional(CONF_ON_CLICK): vol.All(cv.ensure_list, [automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_variable_id(ClickTrigger),
        vol.Optional(CONF_MIN_LENGTH, default='50ms'): cv.positive_time_period_milliseconds,
        vol.Optional(CONF_MAX_LENGTH, default='350ms'): cv.positive_time_period_milliseconds,
    })]),
    vol.Optional(CONF_ON_DOUBLE_CLICK):
        vol.All(cv.ensure_list, [automation.validate_automation({
            cv.GenerateID(CONF_TRIGGER_ID): cv.declare_variable_id(DoubleClickTrigger),
            vol.Optional(CONF_MIN_LENGTH, default='50ms'): cv.positive_time_period_milliseconds,
            vol.Optional(CONF_MAX_LENGTH, default='350ms'): cv.positive_time_period_milliseconds,
        })]),

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


BUILD_FLAGS = '-DUSE_BINARY_SENSOR'
