import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    CONF_IIR_FILTER,
    CONF_OVERSAMPLING,
    CONF_PRESSURE,
    CONF_TEMPERATURE,
    DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_HECTOPASCAL,
)

CODEOWNERS = ["@kahrendt"]
DEPENDENCIES = ["i2c"]

bmp581_ns = cg.esphome_ns.namespace("bmp581")

Oversampling = bmp581_ns.enum("Oversampling")
OVERSAMPLING_OPTIONS = {
    "NONE": Oversampling.OVERSAMPLING_NONE,
    "2X": Oversampling.OVERSAMPLING_X2,
    "4X": Oversampling.OVERSAMPLING_X4,
    "8X": Oversampling.OVERSAMPLING_X8,
    "16X": Oversampling.OVERSAMPLING_X16,
    "32X": Oversampling.OVERSAMPLING_X32,
    "64X": Oversampling.OVERSAMPLING_X64,
    "128X": Oversampling.OVERSAMPLING_X128,
}

IIRFilter = bmp581_ns.enum("IIRFilter")
IIR_FILTER_OPTIONS = {
    "OFF": IIRFilter.IIR_FILTER_OFF,
    "2X": IIRFilter.IIR_FILTER_2,
    "4X": IIRFilter.IIR_FILTER_4,
    "8X": IIRFilter.IIR_FILTER_8,
    "16X": IIRFilter.IIR_FILTER_16,
    "32X": IIRFilter.IIR_FILTER_32,
    "64X": IIRFilter.IIR_FILTER_64,
    "128X": IIRFilter.IIR_FILTER_128,
}

BMP581Component = bmp581_ns.class_(
    "BMP581Component", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BMP581Component),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_OVERSAMPLING, default="NONE"): cv.enum(
                        OVERSAMPLING_OPTIONS, upper=True
                    ),
                    cv.Optional(CONF_IIR_FILTER, default="OFF"): cv.enum(
                        IIR_FILTER_OPTIONS, upper=True
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
                    cv.Optional(CONF_OVERSAMPLING, default="16X"): cv.enum(
                        OVERSAMPLING_OPTIONS, upper=True
                    ),
                    cv.Optional(CONF_IIR_FILTER, default="OFF"): cv.enum(
                        IIR_FILTER_OPTIONS, upper=True
                    ),
                }
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x46))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    if CONF_TEMPERATURE in config:
        conf = config[CONF_TEMPERATURE]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_temperature_sensor(sens))
        cg.add(var.set_temperature_oversampling_config(conf[CONF_OVERSAMPLING]))
        cg.add(var.set_temperature_iir_filter_config(conf[CONF_IIR_FILTER]))

    if CONF_PRESSURE in config:
        conf = config[CONF_PRESSURE]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_pressure_sensor(sens))
        cg.add(var.set_pressure_oversampling_config(conf[CONF_OVERSAMPLING]))
        cg.add(var.set_pressure_iir_filter_config(conf[CONF_IIR_FILTER]))
