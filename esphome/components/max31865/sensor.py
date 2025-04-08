import esphome.codegen as cg
from esphome.components import sensor, spi
import esphome.config_validation as cv
from esphome.const import (
    CONF_MAINS_FILTER,
    CONF_REFERENCE_RESISTANCE,
    CONF_RTD_NOMINAL_RESISTANCE,
    CONF_RTD_WIRES,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)

CODEOWNERS = ["@DAVe3283"]
DEPENDENCIES = ["spi"]

max31865_ns = cg.esphome_ns.namespace("max31865")
MAX31865Sensor = max31865_ns.class_(
    "MAX31865Sensor", sensor.Sensor, cg.PollingComponent, spi.SPIDevice
)

MAX31865ConfigFilter = max31865_ns.enum("MAX31865ConfigFilter")
FILTER = {
    "50HZ": MAX31865ConfigFilter.FILTER_50HZ,
    "60HZ": MAX31865ConfigFilter.FILTER_60HZ,
}

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        MAX31865Sensor,
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.Required(CONF_REFERENCE_RESISTANCE): cv.All(
                cv.resistance, cv.Range(min=100, max=10000)
            ),
            cv.Required(CONF_RTD_NOMINAL_RESISTANCE): cv.All(
                cv.resistance, cv.Range(min=100, max=1000)
            ),
            cv.Optional(CONF_MAINS_FILTER, default="60HZ"): cv.enum(
                FILTER, upper=True, space=""
            ),
            cv.Optional(CONF_RTD_WIRES, default=4): cv.int_range(min=2, max=4),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(spi.spi_device_schema())
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)
    cg.add(var.set_reference_resistance(config[CONF_REFERENCE_RESISTANCE]))
    cg.add(var.set_nominal_resistance(config[CONF_RTD_NOMINAL_RESISTANCE]))
    cg.add(var.set_filter(config[CONF_MAINS_FILTER]))
    cg.add(var.set_num_rtd_wires(config[CONF_RTD_WIRES]))
