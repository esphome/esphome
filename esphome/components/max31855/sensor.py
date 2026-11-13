import esphome.codegen as cg
from esphome.components import sensor, spi
import esphome.config_validation as cv
from esphome.const import (
    CONF_REFERENCE_TEMPERATURE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)
from esphome.types import ConfigType

CONF_LINEARIZE = "linearize"

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
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.Optional(CONF_LINEARIZE, default=False): cv.boolean,
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


async def to_code(config: ConfigType) -> None:
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)
    if config[CONF_LINEARIZE]:
        # Compile the NIST tables in only when a sensor asks for them.
        cg.add_define("USE_MAX31855_LINEARIZE")
        cg.add(var.set_linearize(True))
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_REFERENCE_TEMPERATURE, var.set_reference_sensor)
