import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    CONF_OVERSAMPLING,
    CONF_PRESSURE,
    CONF_TEMPERATURE,
    DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_HECTOPASCAL,
)

DEPENDENCIES = ["i2c"]

spl06_007_ns = cg.esphome_ns.namespace("spl06_007")
SPL06_007Component = spl06_007_ns.class_(
    "SPL06_007Component", cg.PollingComponent, i2c.I2CDevice
)

SPL06_007TemperatureOversampling = spl06_007_ns.enum("SPL06_007TemperatureOversampling")
TEMPERATURE_OVERSAMPLING_OPTIONS = {
    "1X": SPL06_007TemperatureOversampling.SPL06_007_TEMPERATURE_OVERSAMPLING_1X,
    "2X": SPL06_007TemperatureOversampling.SPL06_007_TEMPERATURE_OVERSAMPLING_2X,
    "4X": SPL06_007TemperatureOversampling.SPL06_007_TEMPERATURE_OVERSAMPLING_4X,
    "8X": SPL06_007TemperatureOversampling.SPL06_007_TEMPERATURE_OVERSAMPLING_8X,
    "16X": SPL06_007TemperatureOversampling.SPL06_007_TEMPERATURE_OVERSAMPLING_16X,
    "32X": SPL06_007TemperatureOversampling.SPL06_007_TEMPERATURE_OVERSAMPLING_32X,
    "64X": SPL06_007TemperatureOversampling.SPL06_007_TEMPERATURE_OVERSAMPLING_64X,
    "128X": SPL06_007TemperatureOversampling.SPL06_007_TEMPERATURE_OVERSAMPLING_128X,
}

SPL06_007PressureOversampling = spl06_007_ns.enum("SPL06_007PressureOversampling")
PRESSURE_OVERSAMPLING_OPTIONS = {
    "1X": SPL06_007PressureOversampling.SPL06_007_PRESSURE_OVERSAMPLING_1X,
    "2X": SPL06_007PressureOversampling.SPL06_007_PRESSURE_OVERSAMPLING_2X,
    "4X": SPL06_007PressureOversampling.SPL06_007_PRESSURE_OVERSAMPLING_4X,
    "8X": SPL06_007PressureOversampling.SPL06_007_PRESSURE_OVERSAMPLING_8X,
    "16X": SPL06_007PressureOversampling.SPL06_007_PRESSURE_OVERSAMPLING_16X,
    "32X": SPL06_007PressureOversampling.SPL06_007_PRESSURE_OVERSAMPLING_32X,
    "64X": SPL06_007PressureOversampling.SPL06_007_PRESSURE_OVERSAMPLING_64X,
    "128X": SPL06_007PressureOversampling.SPL06_007_PRESSURE_OVERSAMPLING_128X,
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SPL06_007Component),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_OVERSAMPLING, default="2X"): cv.enum(
                        TEMPERATURE_OVERSAMPLING_OPTIONS, upper=True
                    ),
                }
            ),
            cv.Optional(CONF_PRESSURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_HECTOPASCAL,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_PRESSURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_OVERSAMPLING, default="2X"): cv.enum(
                        PRESSURE_OVERSAMPLING_OPTIONS, upper=True
                    ),
                }
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x76))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if temperature_config := config.get(CONF_TEMPERATURE):
        sens = await sensor.new_sensor(temperature_config)
        cg.add(var.set_temperature_sensor(sens))
        cg.add(var.set_temperature_oversampling(temperature_config[CONF_OVERSAMPLING]))

    if pressure_config := config.get(CONF_PRESSURE):
        sens = await sensor.new_sensor(pressure_config)
        cg.add(var.set_pressure_sensor(sens))
        cg.add(var.set_pressure_oversampling(pressure_config[CONF_OVERSAMPLING]))
