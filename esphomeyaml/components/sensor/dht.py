import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_HUMIDITY, CONF_MAKE_ID, CONF_MODEL, CONF_NAME, CONF_PIN, \
    CONF_TEMPERATURE, CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, Application, add, gpio_output_pin_expression, variable
from esphomeyaml.pins import gpio_output_pin_schema

DHT_MODELS = {
    'AUTO_DETECT': sensor.sensor_ns.DHT_MODEL_AUTO_DETECT,
    'DHT11': sensor.sensor_ns.DHT_MODEL_DHT11,
    'DHT22': sensor.sensor_ns.DHT_MODEL_DHT22,
    'AM2302': sensor.sensor_ns.DHT_MODEL_AM2302,
    'RHT03': sensor.sensor_ns.DHT_MODEL_RHT03,
}

MakeDHTSensor = Application.MakeDHTSensor

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeDHTSensor),
    vol.Required(CONF_PIN): gpio_output_pin_schema,
    vol.Required(CONF_TEMPERATURE): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Required(CONF_HUMIDITY): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_MODEL): vol.All(vol.Upper, cv.one_of(*DHT_MODELS)),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
})


def to_code(config):
    pin = None
    for pin in gpio_output_pin_expression(config[CONF_PIN]):
        yield
    rhs = App.make_dht_sensor(config[CONF_TEMPERATURE][CONF_NAME],
                              config[CONF_HUMIDITY][CONF_NAME],
                              pin, config.get(CONF_UPDATE_INTERVAL))
    dht = variable(config[CONF_MAKE_ID], rhs)
    if CONF_MODEL in config:
        constant = DHT_MODELS[config[CONF_MODEL]]
        add(dht.Pdht.set_dht_model(constant))

    sensor.setup_sensor(dht.Pdht.Pget_temperature_sensor(),
                        dht.Pmqtt_temperature, config[CONF_TEMPERATURE])
    sensor.setup_sensor(dht.Pdht.Pget_humidity_sensor(),
                        dht.Pmqtt_humidity, config[CONF_HUMIDITY])


BUILD_FLAGS = '-DUSE_DHT_SENSOR'
