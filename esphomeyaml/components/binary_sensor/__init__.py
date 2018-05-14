import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_DEVICE_CLASS, CONF_INVERTED, CONF_MQTT_ID
from esphomeyaml.helpers import add, setup_mqtt_component, App, Pvariable

DEVICE_CLASSES = [
    '', 'battery', 'cold', 'connectivity', 'door', 'garage_door', 'gas',
    'heat', 'light', 'lock', 'moisture', 'motion', 'moving', 'occupancy',
    'opening', 'plug', 'power', 'presence', 'problem', 'safety', 'smoke',
    'sound', 'vibration', 'window'
]

DEVICE_CLASSES_MSG = "Unknown device class. Must be one of {}".format(', '.join(DEVICE_CLASSES))

PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({
    vol.Optional(CONF_INVERTED): cv.boolean,
    vol.Optional(CONF_DEVICE_CLASS): vol.All(vol.Lower,
                                             vol.Any(*DEVICE_CLASSES, msg=DEVICE_CLASSES_MSG)),
})

MQTT_BINARY_SENSOR_SCHEMA = cv.MQTT_COMPONENT_SCHEMA.extend({

})

MQTT_BINARY_SENSOR_ID_SCHEMA = MQTT_BINARY_SENSOR_SCHEMA.extend({
    cv.GenerateID('mqtt_binary_sensor', CONF_MQTT_ID): cv.register_variable_id,
})


def setup_binary_sensor(obj, config):
    if CONF_DEVICE_CLASS in config:
        add(obj.set_device_class(config[CONF_DEVICE_CLASS]))
    if CONF_INVERTED in config:
        add(obj.set_inverted(config[CONF_INVERTED]))


def setup_mqtt_binary_sensor(obj, config):
    setup_mqtt_component(obj, config)


def register_binary_sensor(var, config):
    setup_binary_sensor(var, config)
    rhs = App.register_binary_sensor(var)
    mqtt_sensor = Pvariable('binary_sensor::MQTTBinarySensorComponent', config[CONF_MQTT_ID], rhs)
    setup_mqtt_binary_sensor(mqtt_sensor, config)


BUILD_FLAGS = '-DUSE_BINARY_SENSOR'
