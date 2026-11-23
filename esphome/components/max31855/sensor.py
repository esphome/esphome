import esphome.codegen as cg
from esphome.components import sensor, spi
import esphome.config_validation as cv
from esphome.const import CONF_REFERENCE_TEMPERATURE
from esphome.types import ConfigType

max31855_ns = cg.esphome_ns.namespace("max31855")
MAX31855Sensor = max31855_ns.class_(
    "MAX31855Sensor", sensor.Sensor, cg.PollingComponent, spi.SPIDevice
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(MAX31855Sensor, kind=sensor.TEMPERATURE)
    .extend(
        {
            cv.Optional(CONF_REFERENCE_TEMPERATURE): sensor.sensor_schema(
                kind=sensor.TEMPERATURE, accuracy_decimals=2
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(spi.spi_device_schema())
)


async def to_code(config: ConfigType) -> None:
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_REFERENCE_TEMPERATURE, var.set_reference_sensor)
