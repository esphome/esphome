import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.components import sensor, i2c

from esphome.const import (
    CONF_ID,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_MILLION,
)

CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = ["i2c"]

CONF_CARBON_MONOXIDE = "carbon_monoxide"
CONF_NITROGEN_DIOXIDE = "nitrogen_dioxide"
CONF_METHANE = "methane"
CONF_ETHANOL = "ethanol"
CONF_HYDROGEN = "hydrogen"
CONF_AMMONIA = "ammonia"


mics_4514_ns = cg.esphome_ns.namespace("mics_4514")
MICS4514Component = mics_4514_ns.class_(
    "MICS4514Component", cg.PollingComponent, i2c.I2CDevice
)

SENSORS = [
    CONF_CARBON_MONOXIDE,
    CONF_METHANE,
    CONF_ETHANOL,
    CONF_HYDROGEN,
    CONF_AMMONIA,
]

common_sensor_schema = sensor.sensor_schema(
    unit_of_measurement=UNIT_PARTS_PER_MILLION,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=2,
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MICS4514Component),
            cv.Optional(CONF_NITROGEN_DIOXIDE): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_MILLION,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=2,
            ),
        }
    )
    .extend({cv.Optional(sensor_type): common_sensor_schema for sensor_type in SENSORS})
    .extend(i2c.i2c_device_schema(0x75))
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    for sensor_type in SENSORS:
        if sensor_type in config:
            sens = await sensor.new_sensor(config[sensor_type])
            cg.add(getattr(var, f"set_{sensor_type}_sensor")(sens))

    if CONF_NITROGEN_DIOXIDE in config:
        sens = await sensor.new_sensor(config[CONF_NITROGEN_DIOXIDE])
        cg.add(var.set_nitrogen_dioxide_sensor(sens))
