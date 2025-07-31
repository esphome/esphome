import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_FIELD_STRENGTH_X,
    CONF_FIELD_STRENGTH_Y,
    CONF_FIELD_STRENGTH_Z,
    CONF_ID,
    ICON_MAGNET,
    STATE_CLASS_MEASUREMENT,
    UNIT_MICROTESLA,
)

DEPENDENCIES = ["i2c"]

mmc5983_ns = cg.esphome_ns.namespace("mmc5983")
MMC5983Component = mmc5983_ns.class_(
    "MMC5983Component", cg.PollingComponent, i2c.I2CDevice
)

field_strength_schema = sensor.sensor_schema(
    unit_of_measurement=UNIT_MICROTESLA,
    icon=ICON_MAGNET,
    accuracy_decimals=4,
    state_class=STATE_CLASS_MEASUREMENT,
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MMC5983Component),
            cv.Optional(CONF_FIELD_STRENGTH_X): field_strength_schema,
            cv.Optional(CONF_FIELD_STRENGTH_Y): field_strength_schema,
            cv.Optional(CONF_FIELD_STRENGTH_Z): field_strength_schema,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x30))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if x_config := config.get(CONF_FIELD_STRENGTH_X):
        sens = await sensor.new_sensor(x_config)
        cg.add(var.set_x_sensor(sens))
    if y_config := config.get(CONF_FIELD_STRENGTH_Y):
        sens = await sensor.new_sensor(y_config)
        cg.add(var.set_y_sensor(sens))
    if z_config := config.get(CONF_FIELD_STRENGTH_Z):
        sens = await sensor.new_sensor(z_config)
        cg.add(var.set_z_sensor(sens))
