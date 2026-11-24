import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import CONF_HUMIDITY, CONF_TEMPERATURE
from esphome.types import ConfigType

DEPENDENCIES = ["i2c"]

am2315c_ns = cg.esphome_ns.namespace("am2315c")
AM2315C = am2315c_ns.class_("AM2315C", cg.PollingComponent, i2c.I2CDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(AM2315C),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                kind=sensor.TEMPERATURE
            ),
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(kind=sensor.HUMIDITY),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x38))
)


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_TEMPERATURE, var.set_temperature_sensor)
    await sensors(CONF_HUMIDITY, var.set_humidity_sensor)
