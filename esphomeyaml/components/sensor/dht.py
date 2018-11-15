import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_HUMIDITY, CONF_MAKE_ID, CONF_MODEL, CONF_NAME, CONF_PIN, \
    CONF_TEMPERATURE, CONF_UPDATE_INTERVAL, CONF_ID
from esphomeyaml.helpers import App, Application, add, gpio_output_pin_expression, variable, \
    setup_component, PollingComponent, Pvariable
from esphomeyaml.pins import gpio_output_pin_schema

DHTModel = sensor.sensor_ns.enum('DHTModel')
DHT_MODELS = {
    'AUTO_DETECT': DHTModel.DHT_MODEL_AUTO_DETECT,
    'DHT11': DHTModel.DHT_MODEL_DHT11,
    'DHT22': DHTModel.DHT_MODEL_DHT22,
    'AM2302': DHTModel.DHT_MODEL_AM2302,
    'RHT03': DHTModel.DHT_MODEL_RHT03,
}

MakeDHTSensor = Application.struct('MakeDHTSensor')
DHTComponent = sensor.sensor_ns.class_('DHTComponent', PollingComponent)
DHTTemperatureSensor = sensor.sensor_ns.class_('DHTTemperatureSensor',
                                               sensor.EmptyPollingParentSensor)
DHTHumiditySensor = sensor.sensor_ns.class_('DHTHumiditySensor',
                                            sensor.EmptyPollingParentSensor)

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeDHTSensor),
    cv.GenerateID(): cv.declare_variable_id(DHTComponent),
    vol.Required(CONF_PIN): gpio_output_pin_schema,
    vol.Required(CONF_TEMPERATURE): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(DHTTemperatureSensor),
    })),
    vol.Required(CONF_HUMIDITY): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(DHTHumiditySensor),
    })),
    vol.Optional(CONF_MODEL): vol.All(vol.Upper, cv.one_of(*DHT_MODELS)),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    for pin in gpio_output_pin_expression(config[CONF_PIN]):
        yield
    rhs = App.make_dht_sensor(config[CONF_TEMPERATURE][CONF_NAME],
                              config[CONF_HUMIDITY][CONF_NAME],
                              pin, config.get(CONF_UPDATE_INTERVAL))
    make = variable(config[CONF_MAKE_ID], rhs)
    dht = make.Pdht
    Pvariable(config[CONF_ID], dht)

    if CONF_MODEL in config:
        constant = DHT_MODELS[config[CONF_MODEL]]
        add(dht.set_dht_model(constant))

    sensor.setup_sensor(dht.Pget_temperature_sensor(),
                        make.Pmqtt_temperature, config[CONF_TEMPERATURE])
    sensor.setup_sensor(dht.Pget_humidity_sensor(),
                        make.Pmqtt_humidity, config[CONF_HUMIDITY])
    setup_component(dht, config)


BUILD_FLAGS = '-DUSE_DHT_SENSOR'


def to_hass_config(data, config):
    return [sensor.core_to_hass_config(data, config[CONF_TEMPERATURE]),
            sensor.core_to_hass_config(data, config[CONF_HUMIDITY])]
