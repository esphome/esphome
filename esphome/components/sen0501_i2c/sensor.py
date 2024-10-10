import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    ICON_BRIGHTNESS_5,
    ICON_ELEVATION,
    ICON_GAUGE,
    ICON_THERMOMETER,
    ICON_UV_RADIATION,
    ICON_WATER_PERCENT,
    UNIT_CELSIUS,
    UNIT_HECTOPASCAL,
    UNIT_IRRADIANCE,
    UNIT_LUX,
    UNIT_METER,
    UNIT_PERCENT,
    DEVICE_CLASS_ATMOSPHERIC_PRESSURE,
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_ILLUMINANCE,
    DEVICE_CLASS_IRRADIANCE,
    DEVICE_CLASS_TEMPERATURE,
    CONF_HUMIDITY,
    CONF_ID,
    CONF_ILLUMINANCE,
    CONF_TEMPERATURE,
    CONF_PRESSURE,
    CONF_UV_IRRADIANCE,
    STATE_CLASS_MEASUREMENT,
)

CONF_ELEVATION = "elevation"

DEPENDENCIES = ["i2c"]

sen0501_ns = cg.esphome_ns.namespace("sen0501_i2c")
Sen0501_i2cComponent = sen0501_ns.class_(
    "Sen0501_i2cComponent", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Sen0501_i2cComponent),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_THERMOMETER,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                icon=ICON_WATER_PERCENT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_UV_IRRADIANCE): sensor.sensor_schema(
                unit_of_measurement=UNIT_IRRADIANCE,
                icon=ICON_UV_RADIATION,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_IRRADIANCE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_ILLUMINANCE): sensor.sensor_schema(
                unit_of_measurement=UNIT_LUX,
                icon=ICON_BRIGHTNESS_5,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_ILLUMINANCE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PRESSURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_HECTOPASCAL,
                icon=ICON_GAUGE,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_ATMOSPHERIC_PRESSURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_ELEVATION): sensor.sensor_schema(
                unit_of_measurement=UNIT_METER,
                icon=ICON_ELEVATION,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_DISTANCE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x22))
)


async def to_code(config):

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if temperature_config := config.get(CONF_TEMPERATURE):
        sens = await sensor.new_sensor(temperature_config)
        cg.add(var.set_temperature(sens))

    if humidity_config := config.get(CONF_HUMIDITY):
        sens = await sensor.new_sensor(humidity_config)
        cg.add(var.set_humidity(sens))

    if uv_irradiance_config := config.get(CONF_UV_IRRADIANCE):
        sens = await sensor.new_sensor(uv_irradiance_config)
        cg.add(var.set_uv_intensity(sens))

    if brightness_config := config.get(CONF_ILLUMINANCE):
        sens = await sensor.new_sensor(brightness_config)
        cg.add(var.set_luminous_intensity(sens))

    if pressure_config := config.get(CONF_PRESSURE):
        sens = await sensor.new_sensor(pressure_config)
        cg.add(var.set_atmospheric_pressure(sens))

    if elevation_config := config.get(CONF_ELEVATION):
        sens = await sensor.new_sensor(elevation_config)
        cg.add(var.set_elevation(sens))
