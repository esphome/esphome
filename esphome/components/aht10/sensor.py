import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import CONF_HUMIDITY, CONF_TEMPERATURE, CONF_VARIANT
from esphome.types import ConfigType

DEPENDENCIES = ["i2c"]

aht10_ns = cg.esphome_ns.namespace("aht10")
AHT10Component = aht10_ns.class_("AHT10Component", cg.PollingComponent, i2c.I2CDevice)

AHT10Variant = aht10_ns.enum("AHT10Variant")
AHT10_VARIANTS = AHT10Variant.options("AHT10", "AHT20")

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(AHT10Component),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                kind=sensor.TEMPERATURE, accuracy_decimals=2
            ),
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
                kind=sensor.HUMIDITY, accuracy_decimals=2
            ),
            cv.Optional(CONF_VARIANT, default="AHT10"): cv.enum(
                AHT10_VARIANTS, upper=True
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x38))
)


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)
    cg.add(var.set_variant(config[CONF_VARIANT]))

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_TEMPERATURE, var.set_temperature_sensor)
    await sensors(CONF_HUMIDITY, var.set_humidity_sensor)
