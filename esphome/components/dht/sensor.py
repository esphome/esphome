from esphome import pins
import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_HUMIDITY,
    CONF_ID,
    CONF_MODEL,
    CONF_PIN,
    CONF_TEMPERATURE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
)
from esphome.cpp_helpers import gpio_pin_expression

dht_ns = cg.esphome_ns.namespace("dht")
DHTModel = dht_ns.enum("DHTModel")
DHT_MODELS = {
    "AUTO_DETECT": DHTModel.DHT_MODEL_AUTO_DETECT,
    "DHT11": DHTModel.DHT_MODEL_DHT11,
    "DHT22": DHTModel.DHT_MODEL_DHT22,
    "AM2120": DHTModel.DHT_MODEL_AM2120,
    "AM2302": DHTModel.DHT_MODEL_AM2302,
    "RHT03": DHTModel.DHT_MODEL_RHT03,
    "SI7021": DHTModel.DHT_MODEL_SI7021,
    "DHT22_TYPE2": DHTModel.DHT_MODEL_DHT22_TYPE2,
}
DHT = dht_ns.class_("DHT", cg.PollingComponent)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DHT),
        cv.Required(CONF_PIN): pins.internal_gpio_input_pullup_pin_schema,
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_HUMIDITY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_MODEL, default="auto detect"): cv.enum(
            DHT_MODELS, upper=True, space="_"
        ),
    }
).extend(cv.polling_component_schema("60s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    pin = await gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))

    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))
    if CONF_HUMIDITY in config:
        sens = await sensor.new_sensor(config[CONF_HUMIDITY])
        cg.add(var.set_humidity_sensor(sens))

    cg.add(var.set_dht_model(config[CONF_MODEL]))
