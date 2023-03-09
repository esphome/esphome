import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    CONF_PRESSURE,
    CONF_TEMPERATURE,
    CONF_INTERNAL_FILTER_MODE,
    DEVICE_CLASS_ATMOSPHERIC_PRESSURE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_HECTOPASCAL,
)

DEPENDENCIES = ["i2c"]

hp206c_ns = cg.esphome_ns.namespace("hp206c")
HP206CComponent = hp206c_ns.class_(
    "HP206CComponent", cg.PollingComponent, i2c.I2CDevice
)

HP206CFilterMode = hp206c_ns.enum("InternalFilterMode")
FILTER_MODES = {
    "128": HP206CFilterMode.FILTER_OSR_128,
    "256": HP206CFilterMode.FILTER_OSR_256,
    "512": HP206CFilterMode.FILTER_OSR_512,
    "1024": HP206CFilterMode.FILTER_OSR_1024,
    "2048": HP206CFilterMode.FILTER_OSR_2048,
    "4096": HP206CFilterMode.FILTER_OSR_4096,
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(HP206CComponent),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PRESSURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_HECTOPASCAL,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_ATMOSPHERIC_PRESSURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_INTERNAL_FILTER_MODE, default="2048"): cv.enum(
                FILTER_MODES, upper=True
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x76))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_filter_mode(config[CONF_INTERNAL_FILTER_MODE]))

    if CONF_TEMPERATURE in config:
        conf = config[CONF_TEMPERATURE]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_temperature(sens))

    if CONF_PRESSURE in config:
        conf = config[CONF_PRESSURE]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_pressure(sens))
