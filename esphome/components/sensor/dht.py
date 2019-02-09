import voluptuous as vol

from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_HUMIDITY, CONF_ID, CONF_MODEL, CONF_NAME, \
    CONF_PIN, CONF_TEMPERATURE, CONF_UPDATE_INTERVAL
from esphome.cpp_generator import Pvariable, add
from esphome.cpp_helpers import gpio_output_pin_expression, setup_component
from esphome.cpp_types import App, PollingComponent
from esphome.pins import gpio_input_pullup_pin_schema

DHTModel = sensor.sensor_ns.enum('DHTModel')
DHT_MODELS = {
    'AUTO_DETECT': DHTModel.DHT_MODEL_AUTO_DETECT,
    'DHT11': DHTModel.DHT_MODEL_DHT11,
    'DHT22': DHTModel.DHT_MODEL_DHT22,
    'AM2302': DHTModel.DHT_MODEL_AM2302,
    'RHT03': DHTModel.DHT_MODEL_RHT03,
    'SI7021': DHTModel.DHT_MODEL_SI7021,
}

DHTComponent = sensor.sensor_ns.class_('DHTComponent', PollingComponent)
DHTTemperatureSensor = sensor.sensor_ns.class_('DHTTemperatureSensor',
                                               sensor.EmptyPollingParentSensor)
DHTHumiditySensor = sensor.sensor_ns.class_('DHTHumiditySensor',
                                            sensor.EmptyPollingParentSensor)

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(DHTComponent),
    vol.Required(CONF_PIN): gpio_input_pullup_pin_schema,
    vol.Required(CONF_TEMPERATURE): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(DHTTemperatureSensor),
    })),
    vol.Required(CONF_HUMIDITY): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(DHTHumiditySensor),
    })),
    vol.Optional(CONF_MODEL): cv.one_of(*DHT_MODELS, upper=True),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    for pin in gpio_output_pin_expression(config[CONF_PIN]):
        yield
    rhs = App.make_dht_sensor(config[CONF_TEMPERATURE][CONF_NAME],
                              config[CONF_HUMIDITY][CONF_NAME],
                              pin, config.get(CONF_UPDATE_INTERVAL))
    dht = Pvariable(config[CONF_ID], rhs)

    if CONF_MODEL in config:
        constant = DHT_MODELS[config[CONF_MODEL]]
        add(dht.set_dht_model(constant))

    sensor.setup_sensor(dht.Pget_temperature_sensor(), config[CONF_TEMPERATURE])
    sensor.setup_sensor(dht.Pget_humidity_sensor(), config[CONF_HUMIDITY])
    setup_component(dht, config)


BUILD_FLAGS = '-DUSE_DHT_SENSOR'


def to_hass_config(data, config):
    return [sensor.core_to_hass_config(data, config[CONF_TEMPERATURE]),
            sensor.core_to_hass_config(data, config[CONF_HUMIDITY])]
