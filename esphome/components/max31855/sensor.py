import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, spi
from esphome.const import (
    CONF_REFERENCE_TEMPERATURE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)

max31855_ns = cg.esphome_ns.namespace("max31855")
MAX31855Sensor = max31855_ns.class_(
    "MAX31855Sensor", sensor.Sensor, cg.PollingComponent, spi.SPIDevice
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        MAX31855Sensor,
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
    )
    .extend(
        {
            cv.Optional(CONF_REFERENCE_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(spi.spi_device_schema())
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)
    if CONF_REFERENCE_TEMPERATURE in config:
        tc_ref = await sensor.new_sensor(config[CONF_REFERENCE_TEMPERATURE])
        cg.add(var.set_reference_sensor(tc_ref))
