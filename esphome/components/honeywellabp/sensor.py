import esphome.codegen as cg
from esphome.components import sensor, spi
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_PRESSURE,
    CONF_TEMPERATURE,
    DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)

DEPENDENCIES = ["spi"]
CODEOWNERS = ["@RubyBailey"]

CONF_MIN_PRESSURE = "min_pressure"
CONF_MAX_PRESSURE = "max_pressure"

honeywellabp_ns = cg.esphome_ns.namespace("honeywellabp")
HONEYWELLABPSensor = honeywellabp_ns.class_(
    "HONEYWELLABPSensor", sensor.Sensor, cg.PollingComponent, spi.SPIDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(HONEYWELLABPSensor),
            cv.Optional(CONF_PRESSURE): sensor.sensor_schema(
                unit_of_measurement="psi",
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_PRESSURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Required(CONF_MIN_PRESSURE): cv.float_,
                    cv.Required(CONF_MAX_PRESSURE): cv.float_,
                }
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(spi.spi_device_schema(cs_pin_required=True))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

    if CONF_PRESSURE in config:
        conf = config[CONF_PRESSURE]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_pressure_sensor(sens))
        cg.add(var.set_honeywellabp_min_pressure(conf[CONF_MIN_PRESSURE]))
        cg.add(var.set_honeywellabp_max_pressure(conf[CONF_MAX_PRESSURE]))

    if CONF_TEMPERATURE in config:
        conf = config[CONF_TEMPERATURE]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_temperature_sensor(sens))
