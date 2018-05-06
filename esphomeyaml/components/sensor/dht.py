import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.components import sensor
from esphomeyaml.components.sensor import MQTT_SENSOR_SCHEMA
from esphomeyaml.const import CONF_HUMIDITY, CONF_ID, CONF_MODEL, CONF_NAME, CONF_PIN, \
    CONF_TEMPERATURE, CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, RawExpression, add, variable

DHT_MODELS = ['AUTO_DETECT', 'DHT11', 'DHT22', 'AM2302', 'RHT03']

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID('dht_sensor'): cv.register_variable_id,
    vol.Required(CONF_PIN): pins.input_output_pin,
    vol.Required(CONF_TEMPERATURE): MQTT_SENSOR_SCHEMA,
    vol.Required(CONF_HUMIDITY): MQTT_SENSOR_SCHEMA,
    vol.Optional(CONF_MODEL): vol.All(vol.Upper, vol.Any(*DHT_MODELS)),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.positive_not_null_time_period,
})


def to_code(config):
    rhs = App.make_dht_sensor(config[CONF_TEMPERATURE][CONF_NAME],
                              config[CONF_HUMIDITY][CONF_NAME],
                              config[CONF_PIN], config.get(CONF_UPDATE_INTERVAL))
    dht = variable('Application::MakeDHTSensor', config[CONF_ID], rhs)
    if CONF_MODEL in config:
        model = RawExpression('DHT::{}'.format(config[CONF_MODEL]))
        add(dht.Pdht.set_dht_model(model))
    sensor.setup_sensor(dht.Pdht.Pget_temperature_sensor(), config[CONF_TEMPERATURE])
    sensor.setup_mqtt_sensor_component(dht.Pmqtt_temperature, config[CONF_TEMPERATURE])
    sensor.setup_sensor(dht.Pdht.Pget_humidity_sensor(), config[CONF_HUMIDITY])
    sensor.setup_mqtt_sensor_component(dht.Pmqtt_humidity, config[CONF_HUMIDITY])


BUILD_FLAGS = '-DUSE_DHT_SENSOR'
