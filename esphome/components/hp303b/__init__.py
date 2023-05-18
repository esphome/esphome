import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, spi

from esphome.const import (
    CONF_ID,
    CONF_PRESSURE,
    UNIT_HECTOPASCAL,
    DEVICE_CLASS_PRESSURE,
    STATE_CLASS_MEASUREMENT,
)

DEPENDENCIES = ["spi"]
CODEOWNERS = ["@max246"]

hp303b_ns = cg.esphome_ns.namespace("hp303b")
HP303BComponent = hp303b_ns.class_(
    "HP303BComponent", cg.PollingComponent, spi.SPIDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.declare_id(HP303BComponent),
            cv.Exclusive(
                CONF_PRESSURE,
                "pressure",
                f"{CONF_PRESSURE}",
            ): sensor.sensor_schema(
                unit_of_measurement=UNIT_HECTOPASCAL,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_PRESSURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(cs_pin_required=True))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

    if CONF_PRESSURE in config:
        sens = await sensor.new_sensor(config[CONF_PRESSURE])
        cg.add(var.set_pressure(sens))
