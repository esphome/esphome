from esphome.components import sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_HUMIDITY, CONF_ID, CONF_MODEL, CONF_NAME, \
    CONF_PIN, CONF_TEMPERATURE, CONF_UPDATE_INTERVAL, CONF_ACCURACY_DECIMALS, CONF_ICON, \
    ICON_THERMOMETER, CONF_UNIT_OF_MEASUREMENT, UNIT_CELSIUS, ICON_WATER_PERCENT, UNIT_PERCENT
from esphome.cpp_helpers import gpio_pin_expression
from esphome.pins import gpio_input_pullup_pin_schema

dht_ns = cg.esphome_ns.namespace('dht')
DHTModel = dht_ns.enum('DHTModel')
DHT_MODELS = {
    'AUTO_DETECT': DHTModel.DHT_MODEL_AUTO_DETECT,
    'DHT11': DHTModel.DHT_MODEL_DHT11,
    'DHT22': DHTModel.DHT_MODEL_DHT22,
    'AM2302': DHTModel.DHT_MODEL_AM2302,
    'RHT03': DHTModel.DHT_MODEL_RHT03,
    'SI7021': DHTModel.DHT_MODEL_SI7021,
}
DHT = dht_ns.class_('DHT', cg.PollingComponent)

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(DHT),
    cv.Required(CONF_PIN): gpio_input_pullup_pin_schema,
    cv.Required(CONF_TEMPERATURE): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.Optional(CONF_ACCURACY_DECIMALS, default=1): sensor.accuracy_decimals,
        cv.Optional(CONF_ICON, default=ICON_THERMOMETER): sensor.icon,
        cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_CELSIUS): sensor.unit_of_measurement,
    })),
    cv.Required(CONF_HUMIDITY): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.Optional(CONF_ACCURACY_DECIMALS, default=0): sensor.accuracy_decimals,
        cv.Optional(CONF_ICON, default=ICON_WATER_PERCENT): sensor.icon,
        cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_PERCENT): sensor.unit_of_measurement,
    })),
    cv.Optional(CONF_MODEL, default='auto detect'): cv.one_of(*DHT_MODELS, upper=True, space='_'),
    cv.Optional(CONF_UPDATE_INTERVAL, default='60s'): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    pin = yield gpio_pin_expression(config[CONF_PIN])
    rhs = DHT.new(config[CONF_TEMPERATURE][CONF_NAME],
                  config[CONF_HUMIDITY][CONF_NAME],
                  pin, config[CONF_UPDATE_INTERVAL])
    dht = cg.Pvariable(config[CONF_ID], rhs)
    yield cg.register_component(dht, config)
    yield sensor.register_sensor(dht.Pget_temperature_sensor(), config[CONF_TEMPERATURE])
    yield sensor.register_sensor(dht.Pget_humidity_sensor(), config[CONF_HUMIDITY])

    cg.add(dht.set_dht_model(DHT_MODELS[config[CONF_MODEL]]))
