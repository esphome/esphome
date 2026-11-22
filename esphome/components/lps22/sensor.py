import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import CONF_PRESSURE, CONF_TEMPERATURE, ICON_THERMOMETER
from esphome.types import ConfigType

CODEOWNERS = ["@nagisa"]
DEPENDENCIES = ["i2c"]

lps22 = cg.esphome_ns.namespace("lps22")

LPS22Component = lps22.class_("LPS22Component", cg.PollingComponent, i2c.I2CDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LPS22Component),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                kind=sensor.TEMPERATURE, icon=ICON_THERMOMETER, accuracy_decimals=2
            ),
            cv.Optional(CONF_PRESSURE): sensor.sensor_schema(
                kind=sensor.PRESSURE, accuracy_decimals=2
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x5D))  # can also be 0x5C
)


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_TEMPERATURE, var.set_temperature_sensor)
    await sensors(CONF_PRESSURE, var.set_pressure_sensor)
