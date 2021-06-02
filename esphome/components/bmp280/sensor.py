import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    CONF_PRESSURE,
    CONF_TEMPERATURE,
    DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    ICON_EMPTY,
    UNIT_HECTOPASCAL,
    CONF_IIR_FILTER,
    CONF_OVERSAMPLING,
)

DEPENDENCIES = ["i2c"]

bmp280_ns = cg.esphome_ns.namespace("bmp280")
BMP280Oversampling = bmp280_ns.enum("BMP280Oversampling")
OVERSAMPLING_OPTIONS = {
    "NONE": BMP280Oversampling.BMP280_OVERSAMPLING_NONE,
    "1X": BMP280Oversampling.BMP280_OVERSAMPLING_1X,
    "2X": BMP280Oversampling.BMP280_OVERSAMPLING_2X,
    "4X": BMP280Oversampling.BMP280_OVERSAMPLING_4X,
    "8X": BMP280Oversampling.BMP280_OVERSAMPLING_8X,
    "16X": BMP280Oversampling.BMP280_OVERSAMPLING_16X,
}

BMP280IIRFilter = bmp280_ns.enum("BMP280IIRFilter")
IIR_FILTER_OPTIONS = {
    "OFF": BMP280IIRFilter.BMP280_IIR_FILTER_OFF,
    "2X": BMP280IIRFilter.BMP280_IIR_FILTER_2X,
    "4X": BMP280IIRFilter.BMP280_IIR_FILTER_4X,
    "8X": BMP280IIRFilter.BMP280_IIR_FILTER_8X,
    "16X": BMP280IIRFilter.BMP280_IIR_FILTER_16X,
}

BMP280Component = bmp280_ns.class_(
    "BMP280Component", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BMP280Component),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                UNIT_CELSIUS,
                ICON_EMPTY,
                1,
                DEVICE_CLASS_TEMPERATURE,
                STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_OVERSAMPLING, default="16X"): cv.enum(
                        OVERSAMPLING_OPTIONS, upper=True
                    ),
                }
            ),
            cv.Optional(CONF_PRESSURE): sensor.sensor_schema(
                UNIT_HECTOPASCAL,
                ICON_EMPTY,
                1,
                DEVICE_CLASS_PRESSURE,
                STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_OVERSAMPLING, default="16X"): cv.enum(
                        OVERSAMPLING_OPTIONS, upper=True
                    ),
                }
            ),
            cv.Optional(CONF_IIR_FILTER, default="OFF"): cv.enum(
                IIR_FILTER_OPTIONS, upper=True
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x77))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_TEMPERATURE in config:
        conf = config[CONF_TEMPERATURE]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_temperature_sensor(sens))
        cg.add(var.set_temperature_oversampling(conf[CONF_OVERSAMPLING]))

    if CONF_PRESSURE in config:
        conf = config[CONF_PRESSURE]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_pressure_sensor(sens))
        cg.add(var.set_pressure_oversampling(conf[CONF_OVERSAMPLING]))
