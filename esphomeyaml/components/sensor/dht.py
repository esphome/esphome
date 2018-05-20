import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_HUMIDITY, CONF_MAKE_ID, CONF_MODEL, CONF_NAME, CONF_PIN, \
    CONF_TEMPERATURE, CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, Application, add, gpio_output_pin_expression, variable
from esphomeyaml.pins import GPIO_OUTPUT_PIN_SCHEMA

DHT_MODELS = {
    'AUTO_DETECT': sensor.sensor_ns.DHT_MODEL_AUTO_DETECT,
    'DHT11': sensor.sensor_ns.DHT_MODEL_DHT11,
    'DHT22': sensor.sensor_ns.DHT_MODEL_DHT22,
    'AM2302': sensor.sensor_ns.DHT_MODEL_AM2302,
    'RHT03': sensor.sensor_ns.DHT_MODEL_RHT03,
}

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID('dht_sensor', CONF_MAKE_ID): cv.register_variable_id,
    vol.Required(CONF_PIN): GPIO_OUTPUT_PIN_SCHEMA,
    vol.Required(CONF_TEMPERATURE): sensor.SENSOR_SCHEMA,
    vol.Required(CONF_HUMIDITY): sensor.SENSOR_SCHEMA,
    vol.Optional(CONF_MODEL): vol.All(vol.Upper, cv.one_of(*DHT_MODELS)),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.positive_time_period_milliseconds,
})

MakeDHTSensor = Application.MakeDHTSensor


def to_code(config):
    pin = gpio_output_pin_expression(config[CONF_PIN])
    rhs = App.make_dht_sensor(config[CONF_TEMPERATURE][CONF_NAME],
                              config[CONF_HUMIDITY][CONF_NAME],
                              pin, config.get(CONF_UPDATE_INTERVAL))
    dht = variable(MakeDHTSensor, config[CONF_MAKE_ID], rhs)
    if CONF_MODEL in config:
        constant = DHT_MODELS[config[CONF_MODEL]]
        add(dht.Pdht.set_dht_model(constant))

    sensor.setup_sensor(dht.Pdht.Pget_temperature_sensor(),
                        dht.Pmqtt_temperature, config[CONF_TEMPERATURE])
    sensor.setup_sensor(dht.Pdht.Pget_humidity_sensor(),
                        dht.Pmqtt_humidity, config[CONF_HUMIDITY])


BUILD_FLAGS = '-DUSE_DHT_SENSOR'
