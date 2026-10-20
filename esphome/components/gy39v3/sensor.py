import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_HUMIDITY,
    CONF_ID,
    CONF_ILLUMINANCE,
    CONF_PRESSURE,
    CONF_TEMPERATURE,
    DEVICE_CLASS_ATMOSPHERIC_PRESSURE,
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_ILLUMINANCE,
    DEVICE_CLASS_TEMPERATURE,
    ICON_ACCELERATION_Y,
    ICON_GAUGE,
    ICON_LIGHTBULB,
    ICON_THERMOMETER,
    ICON_WATER_PERCENT,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_LUX,
    UNIT_METER,
    UNIT_PASCAL,
    UNIT_PERCENT,
)

DEPENDENCIES = ["i2c"]

gy39v3_ns = cg.esphome_ns.namespace("gy39v3")
GY39v3Component = gy39v3_ns.class_(
    "GY39v3Component", cg.PollingComponent, i2c.I2CDevice
)

CONF_ELEVATION = "elevation"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(GY39v3Component),
            cv.Optional(CONF_ILLUMINANCE): sensor.sensor_schema(
                GY39v3Component,
                unit_of_measurement=UNIT_LUX,
                icon=ICON_LIGHTBULB,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_ILLUMINANCE,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                GY39v3Component,
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_THERMOMETER,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(),
            cv.Optional(CONF_PRESSURE): sensor.sensor_schema(
                GY39v3Component,
                unit_of_measurement=UNIT_PASCAL,
                icon=ICON_GAUGE,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_ATMOSPHERIC_PRESSURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(),
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
                GY39v3Component,
                unit_of_measurement=UNIT_PERCENT,
                icon=ICON_WATER_PERCENT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(),
            cv.Optional(CONF_ELEVATION): sensor.sensor_schema(
                GY39v3Component,
                unit_of_measurement=UNIT_METER,
                icon=ICON_ACCELERATION_Y,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_DISTANCE,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x5B))
)


async def to_code(config):
    var = await i2c.new_i2c_component(config[CONF_ID])

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_ILLUMINANCE, var.set_illuminance_sensor)
    await sensors(CONF_TEMPERATURE, var.set_temperature_sensor)
    await sensors(CONF_PRESSURE, var.set_pressure_sensor)
    await sensors(CONF_HUMIDITY, var.set_humidity_sensor)
    await sensors(CONF_ELEVATION, var.set_elevation_sensor)
