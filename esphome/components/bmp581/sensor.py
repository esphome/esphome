import math

import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_IIR_FILTER,
    CONF_OVERSAMPLING,
    CONF_PRESSURE,
    CONF_TEMPERATURE,
    DEVICE_CLASS_ATMOSPHERIC_PRESSURE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PASCAL,
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


def compute_measurement_conversion_time(config):
    # - adds up sensor conversion time based on temperature and pressure oversampling rates given in datasheet
    # - returns a rounded up time in ms

    # Page 12 of datasheet
    PRESSURE_OVERSAMPLING_CONVERSION_TIMES = {
        "NONE": 1.0,
        "2X": 1.7,
        "4X": 2.9,
        "8X": 5.4,
        "16X": 10.4,
        "32X": 20.4,
        "64X": 40.4,
        "128X": 80.4,
    }

    # Page 12 of datasheet
    TEMPERATURE_OVERSAMPLING_CONVERSION_TIMES = {
        "NONE": 1.0,
        "2X": 1.1,
        "4X": 1.5,
        "8X": 2.1,
        "16X": 3.3,
        "32X": 5.8,
        "64X": 10.8,
        "128X": 20.8,
    }

    pressure_conversion_time = (
        0.0  # No conversion time necessary without a pressure sensor
    )
    if pressure_config := config.get(CONF_PRESSURE):
        pressure_conversion_time = PRESSURE_OVERSAMPLING_CONVERSION_TIMES[
            pressure_config.get(CONF_OVERSAMPLING)
        ]

    temperature_conversion_time = (
        1.0  # BMP581 always samples the temperature even if only reading pressure
    )
    if temperature_config := config.get(CONF_TEMPERATURE):
        temperature_conversion_time = TEMPERATURE_OVERSAMPLING_CONVERSION_TIMES[
            temperature_config.get(CONF_OVERSAMPLING)
        ]

    # Datasheet indicates a 5% possible error in each conversion time listed
    return math.ceil(1.05 * (pressure_conversion_time + temperature_conversion_time))


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
                unit_of_measurement=UNIT_PASCAL,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_ATMOSPHERIC_PRESSURE,
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
    if temperature_config := config.get(CONF_TEMPERATURE):
        sens = await sensor.new_sensor(temperature_config)
        cg.add(var.set_temperature_sensor(sens))
        cg.add(
            var.set_temperature_oversampling_config(
                temperature_config[CONF_OVERSAMPLING]
            )
        )
        cg.add(
            var.set_temperature_iir_filter_config(temperature_config[CONF_IIR_FILTER])
        )

    if pressure_config := config.get(CONF_PRESSURE):
        sens = await sensor.new_sensor(pressure_config)
        cg.add(var.set_pressure_sensor(sens))
        cg.add(var.set_pressure_oversampling_config(pressure_config[CONF_OVERSAMPLING]))
        cg.add(var.set_pressure_iir_filter_config(pressure_config[CONF_IIR_FILTER]))

    cg.add(var.set_conversion_time(compute_measurement_conversion_time(config)))
