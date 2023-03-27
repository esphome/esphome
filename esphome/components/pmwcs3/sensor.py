import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    CONF_ADDRESS,
    CONF_TEMPERATURE,
    CONF_EC,
    STATE_CLASS_MEASUREMENT,
    ICON_THERMOMETER,
)

CODEOWNERS = ["@SeByDocKy"]
DEPENDENCIES = ["i2c"]

CONF_E25 = "e25"
CONF_VWC = "vwc"

ICON_EPSILON = "mdi:epsilon"
ICON_SIGMA = "mdi:sigma-lower"
ICON_ALPHA = "mdi:alpha-h-circle-outline"

pmwcs3_ns = cg.esphome_ns.namespace("pmwcs3")
PMWCS3Component = pmwcs3_ns.class_(
    "PMWCS3Component", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PMWCS3Component),
            cv.Optional(CONF_ADDRESS): cv.i2c_address,
            cv.Optional(CONF_E25): sensor.sensor_schema(
                icon=ICON_EPSILON,
                accuracy_decimals=3,
                unit_of_measurement="dS/m",
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_EC): sensor.sensor_schema(
                icon=ICON_SIGMA,
                accuracy_decimals=2,
                unit_of_measurement="mS/m",
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                icon=ICON_THERMOMETER,
                accuracy_decimals=3,
                unit_of_measurement="°C",
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_VWC): sensor.sensor_schema(
                icon=ICON_ALPHA,
                accuracy_decimals=3,
                unit_of_measurement="cm3cm−3",
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x63))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_E25 in config:
        sens = await sensor.new_sensor(config[CONF_E25])
        cg.add(var.set_e25_sensor(sens))

    if CONF_EC in config:
        sens = await sensor.new_sensor(config[CONF_EC])
        cg.add(var.set_ec_sensor(sens))

    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))

    if CONF_VWC in config:
        sens = await sensor.new_sensor(config[CONF_VWC])
        cg.add(var.set_vwc_sensor(sens))
