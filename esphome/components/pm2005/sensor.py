"""PM2005/2105 Sensor component for ESPHome."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    CONF_PM_1_0,
    CONF_PM_2_5,
    CONF_PM_10_0,
    CONF_TYPE,
    DEVICE_CLASS_PM1,
    DEVICE_CLASS_PM10,
    DEVICE_CLASS_PM25,
    ICON_CHEMICAL_WEAPON,
    STATE_CLASS_MEASUREMENT,
    UNIT_MICROGRAMS_PER_CUBIC_METER,
)

DEPENDENCIES = ["i2c"]
CODEOWNERS = ["@andrewjswan"]

pm2005_ns = cg.esphome_ns.namespace("pm2005")
PM2005Component = pm2005_ns.class_(
    "PM2005Component", cg.PollingComponent, i2c.I2CDevice
)

SensorType = pm2005_ns.enum("SensorType")
SENSOR_TYPE = {
    "PM2005": SensorType.PM2005,
    "PM2105": SensorType.PM2105,
}


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PM2005Component),
            cv.Optional(CONF_TYPE, default="PM2005"): cv.enum(SENSOR_TYPE, upper=True),
            cv.Optional(CONF_PM_1_0): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_PM1,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PM_2_5): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_PM25,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PM_10_0): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_PM10,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        },
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x28)),
)


async def to_code(config) -> None:
    """Code generation entry point."""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_sensor_type(config[CONF_TYPE]))

    if pm_1_0_config := config.get(CONF_PM_1_0):
        sens = await sensor.new_sensor(pm_1_0_config)
        cg.add(var.set_pm_1_0_sensor(sens))

    if pm_2_5_config := config.get(CONF_PM_2_5):
        sens = await sensor.new_sensor(pm_2_5_config)
        cg.add(var.set_pm_2_5_sensor(sens))

    if pm_10_0_config := config.get(CONF_PM_10_0):
        sens = await sensor.new_sensor(pm_10_0_config)
        cg.add(var.set_pm_10_0_sensor(sens))
