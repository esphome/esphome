import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ALTITUDE,
    CONF_PRESSURE,
    CONF_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_METER,
)
from esphome.types import ConfigType

CODEOWNERS = ["@kbickar"]
DEPENDENCIES = ["i2c"]

mpl3115a2_ns = cg.esphome_ns.namespace("mpl3115a2")
MPL3115A2Component = mpl3115a2_ns.class_(
    "MPL3115A2Component", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MPL3115A2Component),
            cv.Exclusive(
                CONF_PRESSURE,
                "pressure",
                f"{CONF_PRESSURE} and {CONF_ALTITUDE} can't be used together",
            ): sensor.sensor_schema(kind=sensor.PRESSURE),
            cv.Exclusive(
                CONF_ALTITUDE,
                "pressure",
                f"{CONF_PRESSURE} and {CONF_ALTITUDE} can't be used together",
            ): sensor.sensor_schema(
                unit_of_measurement=UNIT_METER,
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                kind=sensor.TEMPERATURE
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x60))
)


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_PRESSURE, var.set_pressure_sensor)
    await sensors(CONF_ALTITUDE, var.set_altitude_sensor)
    await sensors(CONF_TEMPERATURE, var.set_temperature_sensor)
