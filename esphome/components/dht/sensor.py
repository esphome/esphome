from esphome import pins
import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_HUMIDITY, CONF_ID, CONF_MODEL, CONF_PIN, CONF_TEMPERATURE
from esphome.types import ConfigType

dht_ns = cg.esphome_ns.namespace("dht")
DHTModel = dht_ns.enum("DHTModel")
DHT_MODELS = DHTModel.options(
    "AUTO_DETECT",
    "DHT11",
    "DHT22",
    "AM2120",
    "AM2302",
    "RHT03",
    "SI7021",
    "DHT22_TYPE2",
    prefix="DHT_MODEL_",
)
DHT = dht_ns.class_("DHT", cg.PollingComponent)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DHT),
        cv.Required(CONF_PIN): pins.internal_gpio_input_pullup_pin_schema,
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(kind=sensor.TEMPERATURE),
        cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
            kind=sensor.HUMIDITY, accuracy_decimals=0
        ),
        cv.Optional(CONF_MODEL, default="auto detect"): cv.enum(
            DHT_MODELS, upper=True, space="_"
        ),
    }
).extend(cv.polling_component_schema("60s"))


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    await cg.add_pin(var.set_pin, config[CONF_PIN])

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_TEMPERATURE, var.set_temperature_sensor)
    await sensors(CONF_HUMIDITY, var.set_humidity_sensor)

    cg.add(var.set_dht_model(config[CONF_MODEL]))
