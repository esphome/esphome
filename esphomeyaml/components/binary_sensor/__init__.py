import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import automation
from esphomeyaml.const import CONF_DEVICE_CLASS, CONF_ID, CONF_INVERTED, CONF_MAX_LENGTH, \
    CONF_MIN_LENGTH, CONF_MQTT_ID, CONF_ON_CLICK, CONF_ON_DOUBLE_CLICK, CONF_ON_PRESS, \
    CONF_ON_RELEASE, CONF_TRIGGER_ID
from esphomeyaml.helpers import App, NoArg, Pvariable, add, esphomelib_ns, setup_mqtt_component

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
MQTTBinarySensorComponent = binary_sensor_ns.MQTTBinarySensorComponent

BINARY_SENSOR_SCHEMA = cv.MQTT_COMPONENT_SCHEMA.extend({
    cv.GenerateID('mqtt_binary_sensor', CONF_MQTT_ID): cv.register_variable_id,
    cv.GenerateID('binary_sensor'): cv.register_variable_id,
    vol.Optional(CONF_INVERTED): cv.boolean,
    vol.Optional(CONF_DEVICE_CLASS): vol.All(vol.Lower, cv.one_of(*DEVICE_CLASSES)),
    vol.Optional(CONF_ON_PRESS): vol.All(cv.ensure_list, [automation.AUTOMATION_SCHEMA]),
    vol.Optional(CONF_ON_RELEASE): vol.All(cv.ensure_list, [automation.AUTOMATION_SCHEMA]),
    vol.Optional(CONF_ON_CLICK): vol.All(cv.ensure_list, [automation.AUTOMATION_SCHEMA.extend({
        vol.Optional(CONF_MIN_LENGTH, default='50ms'): cv.positive_time_period_milliseconds,
        vol.Optional(CONF_MAX_LENGTH, default='350ms'): cv.positive_time_period_milliseconds,
    })]),
    vol.Optional(CONF_ON_DOUBLE_CLICK):
        vol.All(cv.ensure_list, [automation.AUTOMATION_SCHEMA.extend({
            vol.Optional(CONF_MIN_LENGTH, default='50ms'): cv.positive_time_period_milliseconds,
            vol.Optional(CONF_MAX_LENGTH, default='350ms'): cv.positive_time_period_milliseconds,
        })]),
})


def setup_binary_sensor_core_(binary_sensor_var, mqtt_var, config):
    if CONF_DEVICE_CLASS in config:
        add(binary_sensor_var.set_device_class(config[CONF_DEVICE_CLASS]))
    if CONF_INVERTED in config:
        add(binary_sensor_var.set_inverted(config[CONF_INVERTED]))

    for conf in config.get(CONF_ON_PRESS, []):
        rhs = binary_sensor_var.make_press_trigger()
        trigger = Pvariable(PressTrigger, conf[CONF_TRIGGER_ID], rhs)
        automation.build_automation(trigger, NoArg, conf)

    for conf in config.get(CONF_ON_RELEASE, []):
        rhs = binary_sensor_var.make_release_trigger()
        trigger = Pvariable(ReleaseTrigger, conf[CONF_TRIGGER_ID], rhs)
        automation.build_automation(trigger, NoArg, conf)

    for conf in config.get(CONF_ON_CLICK, []):
        rhs = binary_sensor_var.make_click_trigger(conf[CONF_MIN_LENGTH], conf[CONF_MAX_LENGTH])
        trigger = Pvariable(ClickTrigger, conf[CONF_TRIGGER_ID], rhs)
        automation.build_automation(trigger, NoArg, conf)

    for conf in config.get(CONF_ON_DOUBLE_CLICK, []):
        rhs = binary_sensor_var.make_double_click_trigger(conf[CONF_MIN_LENGTH],
                                                          conf[CONF_MAX_LENGTH])
        trigger = Pvariable(DoubleClickTrigger, conf[CONF_TRIGGER_ID], rhs)
        automation.build_automation(trigger, NoArg, conf)

    setup_mqtt_component(mqtt_var, config)


def setup_binary_sensor(binary_sensor_obj, mqtt_obj, config):
    binary_sensor_var = Pvariable(BinarySensor, config[CONF_ID], binary_sensor_obj,
                                  has_side_effects=False)
    mqtt_var = Pvariable(MQTTBinarySensorComponent, config[CONF_MQTT_ID], mqtt_obj,
                         has_side_effects=False)
    setup_binary_sensor_core_(binary_sensor_var, mqtt_var, config)


def register_binary_sensor(var, config):
    binary_sensor_var = Pvariable(BinarySensor, config[CONF_ID], var,
                                  has_side_effects=True)
    rhs = App.register_binary_sensor(binary_sensor_var)
    mqtt_var = Pvariable(MQTTBinarySensorComponent, config[CONF_MQTT_ID], rhs,
                         has_side_effects=True)
    setup_binary_sensor_core_(binary_sensor_var, mqtt_var, config)


BUILD_FLAGS = '-DUSE_BINARY_SENSOR'
