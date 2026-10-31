"""PM2005/2105 Sensor component for ESPHome."""

import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_PM_1_0,
    CONF_PM_2_5,
    CONF_PM_10_0,
    CONF_TYPE,
    ICON_CHEMICAL_WEAPON,
)
from esphome.types import ConfigType

DEPENDENCIES = ["i2c"]
CODEOWNERS = ["@andrewjswan"]

pm2005_ns = cg.esphome_ns.namespace("pm2005")
PM2005Component = pm2005_ns.class_(
    "PM2005Component", cg.PollingComponent, i2c.I2CDevice
)

SensorType = pm2005_ns.enum("SensorType")
SENSOR_TYPE = SensorType.options("PM2005", "PM2105")


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PM2005Component),
            cv.Optional(CONF_TYPE, default="PM2005"): cv.enum(SENSOR_TYPE, upper=True),
            cv.Optional(CONF_PM_1_0): sensor.sensor_schema(
                kind=sensor.PM1, accuracy_decimals=0
            ),
            cv.Optional(CONF_PM_2_5): sensor.sensor_schema(
                kind=sensor.PM25, icon=ICON_CHEMICAL_WEAPON, accuracy_decimals=0
            ),
            cv.Optional(CONF_PM_10_0): sensor.sensor_schema(
                kind=sensor.PM10, accuracy_decimals=0
            ),
        },
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x28)),
)


async def to_code(config: ConfigType) -> None:
    """Code generation entry point."""
    var = await i2c.new_i2c_component(config)

    cg.add(var.set_sensor_type(config[CONF_TYPE]))

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_PM_1_0, var.set_pm_1_0_sensor)
    await sensors(CONF_PM_2_5, var.set_pm_2_5_sensor)
    await sensors(CONF_PM_10_0, var.set_pm_10_0_sensor)
