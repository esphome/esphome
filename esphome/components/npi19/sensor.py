import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import CONF_TEMPERATURE, STATE_CLASS_MEASUREMENT
from esphome.types import ConfigType

CODEOWNERS = ["@bakerkj"]
DEPENDENCIES = ["i2c"]

npi19_ns = cg.esphome_ns.namespace("npi19")

NPI19Component = npi19_ns.class_("NPI19Component", cg.PollingComponent, i2c.I2CDevice)

CONF_RAW_PRESSURE = "raw_pressure"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(NPI19Component),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                kind=sensor.TEMPERATURE, accuracy_decimals=1
            ),
            cv.Optional(CONF_RAW_PRESSURE): sensor.sensor_schema(
                accuracy_decimals=0, state_class=STATE_CLASS_MEASUREMENT
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x28))
)


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_TEMPERATURE, var.set_temperature_sensor)
    await sensors(CONF_RAW_PRESSURE, var.set_raw_pressure_sensor)
