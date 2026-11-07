import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ALTITUDE,
    CONF_HUMIDITY,
    CONF_ILLUMINANCE,
    CONF_PRESSURE,
    CONF_TEMPERATURE,
    DEVICE_CLASS_ATMOSPHERIC_PRESSURE,
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_ILLUMINANCE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_LUX,
    UNIT_METER,
    UNIT_PASCAL,
    UNIT_PERCENT,
)
from esphome.types import ConfigType

DEPENDENCIES = ["i2c"]

gy39_ns = cg.esphome_ns.namespace("gy39")
GY39 = gy39_ns.class_("GY39", cg.PollingComponent, i2c.I2CDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(GY39),
            cv.Optional(CONF_ILLUMINANCE): sensor.sensor_schema(
                unit_of_measurement=UNIT_LUX,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_ILLUMINANCE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PRESSURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_PASCAL,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_ATMOSPHERIC_PRESSURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_ALTITUDE): sensor.sensor_schema(
                unit_of_measurement=UNIT_METER,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_DISTANCE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x5B))
)


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_ILLUMINANCE, var.set_illuminance_sensor)
    await sensors(CONF_TEMPERATURE, var.set_temperature_sensor)
    await sensors(CONF_PRESSURE, var.set_pressure_sensor)
    await sensors(CONF_HUMIDITY, var.set_humidity_sensor)
    await sensors(CONF_ALTITUDE, var.set_altitude_sensor)
