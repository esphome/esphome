import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.components import binary_sensor
from esphomeyaml.const import CONF_DEVICE_CLASS, CONF_ID, CONF_INVERTED, CONF_NAME, CONF_PIN
from esphomeyaml.helpers import App, add, exp_gpio_input_pin, variable

PLATFORM_SCHEMA = binary_sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID('gpio_binary_sensor'): cv.register_variable_id,
    vol.Required(CONF_PIN): pins.GPIO_INPUT_PIN_SCHEMA
}).extend(binary_sensor.MQTT_BINARY_SENSOR_SCHEMA.schema)


def to_code(config):
    rhs = App.make_gpio_binary_sensor(exp_gpio_input_pin(config[CONF_PIN]),
                                      config[CONF_NAME], config.get(CONF_DEVICE_CLASS))
    gpio = variable('Application::SimpleBinarySensor', config[CONF_ID], rhs)
    if CONF_INVERTED in config:
        add(gpio.Pgpio.set_inverted(config[CONF_INVERTED]))
    binary_sensor.setup_mqtt_binary_sensor(gpio.Pmqtt, config, skip_device_class=True)
