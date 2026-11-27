import esphome.codegen as cg
from esphome.components import i2c, sensor
from esphome.components.const import CONF_AQI
import esphome.config_validation as cv
from esphome.const import CONF_PM_1_0, CONF_PM_2_5, CONF_PM_10_0
from esphome.types import ConfigType

DEPENDENCIES = ["i2c"]
CODEOWNERS = ["@freekode"]

hm3301_ns = cg.esphome_ns.namespace("hm3301")
HM3301Component = hm3301_ns.class_(
    "HM3301Component", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(HM3301Component),
            cv.Optional(CONF_PM_1_0): sensor.sensor_schema(kind=sensor.PM1),
            cv.Optional(CONF_PM_2_5): sensor.sensor_schema(kind=sensor.PM25),
            cv.Optional(CONF_PM_10_0): sensor.sensor_schema(kind=sensor.PM10),
            # Removed in 2026.12.0 - kept to provide helpful error message
            cv.Optional(CONF_AQI): cv.invalid(
                "The 'aqi' option of hm3301 was removed in ESPHome 2026.12.0. "
                "Give 'pm_2_5' and 'pm_10_0' an 'id' and add a sensor with "
                "'platform: aqi' that uses them."
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x40))
)


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_PM_1_0, var.set_pm_1_0_sensor)
    await sensors(CONF_PM_2_5, var.set_pm_2_5_sensor)
    await sensors(CONF_PM_10_0, var.set_pm_10_0_sensor)
