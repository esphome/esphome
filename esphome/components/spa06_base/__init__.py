import math

import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_OVERSAMPLING,
    CONF_PRESSURE,
    CONF_SAMPLE_RATE,
    CONF_TEMPERATURE,
    DEVICE_CLASS_ATMOSPHERIC_PRESSURE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PASCAL,
)

CODEOWNERS = ["@danielkent-net"]

spa06_ns = cg.esphome_ns.namespace("spa06_base")

SampleRate = spa06_ns.enum("SampleRate")
SAMPLE_RATE_OPTIONS = {
    "1": SampleRate.SAMPLE_RATE_1,
    "2": SampleRate.SAMPLE_RATE_2,
    "4": SampleRate.SAMPLE_RATE_4,
    "8": SampleRate.SAMPLE_RATE_8,
    "16": SampleRate.SAMPLE_RATE_16,
    "32": SampleRate.SAMPLE_RATE_32,
    "64": SampleRate.SAMPLE_RATE_64,
    "128": SampleRate.SAMPLE_RATE_128,
    "25p8": SampleRate.SAMPLE_RATE_25P8,
    "25p4": SampleRate.SAMPLE_RATE_25P4,
    "25p2": SampleRate.SAMPLE_RATE_25P2,
    "25": SampleRate.SAMPLE_RATE_25,
    "50": SampleRate.SAMPLE_RATE_50,
    "100": SampleRate.SAMPLE_RATE_100,
    "200": SampleRate.SAMPLE_RATE_200,
}

Oversampling = spa06_ns.enum("Oversampling")
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

SPA06Component = spa06_ns.class_("SPA06Component", cg.PollingComponent)


def compute_measurement_conversion_time(config):
    # - adds up sensor conversion time based on temperature and pressure oversampling rates given in datasheet
    # - returns a rounded up time in ms

    # Page 26 of datasheet
    PRESSURE_OVERSAMPLING_CONVERSION_TIMES = {
        "NONE": 3.6,
        "2X": 5.2,
        "4X": 8.4,
        "8X": 14.8,
        "16X": 27.6,
        "32X": 53.2,
        "64X": 104.4,
        "128X": 206.8,
    }

    # Datasheet does not have a table for temperature oversampling, but we
    # assume it is the same as for pressure
    TEMPERATURE_OVERSAMPLING_CONVERSION_TIMES = {
        "NONE": 3.6,
        "2X": 5.2,
        "4X": 8.4,
        "8X": 14.8,
        "16X": 27.6,
        "32X": 53.2,
        "64X": 104.4,
        "128X": 206.8,
    }

    pressure_conversion_time = (
        0.0  # No conversion time necessary without a pressure sensor
    )
    if pressure_config := config.get(CONF_PRESSURE):
        pressure_conversion_time = PRESSURE_OVERSAMPLING_CONVERSION_TIMES[
            pressure_config.get(CONF_OVERSAMPLING)
        ]

    temperature_conversion_time = 1.0
    if temperature_config := config.get(CONF_TEMPERATURE):
        temperature_conversion_time = TEMPERATURE_OVERSAMPLING_CONVERSION_TIMES[
            temperature_config.get(CONF_OVERSAMPLING)
        ]

    # TODO: Read datasheet to find conversion time error
    return math.ceil(1.05 * (pressure_conversion_time + temperature_conversion_time))


CONFIG_SCHEMA_BASE = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SPA06Component),
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
                cv.Optional(CONF_SAMPLE_RATE, default="1"): cv.enum(
                    SAMPLE_RATE_OPTIONS, lower=True
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
                cv.Optional(CONF_SAMPLE_RATE, default="1"): cv.enum(
                    SAMPLE_RATE_OPTIONS, lower=True
                ),
            }
        ),
    }
).extend(cv.polling_component_schema("60s"))


async def to_code_base(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    if temperature_config := config.get(CONF_TEMPERATURE):
        sens = await sensor.new_sensor(temperature_config)
        cg.add(var.set_temperature_sensor(sens))
        cg.add(
            var.set_temperature_oversampling_config(
                temperature_config[CONF_OVERSAMPLING]
            )
        )
        cg.add(
            var.set_temperature_sample_rate_config(temperature_config[CONF_SAMPLE_RATE])
        )

    if pressure_config := config.get(CONF_PRESSURE):
        sens = await sensor.new_sensor(pressure_config)
        cg.add(var.set_pressure_sensor(sens))
        cg.add(var.set_pressure_oversampling_config(pressure_config[CONF_OVERSAMPLING]))
        cg.add(var.set_pressure_sample_rate_config(pressure_config[CONF_SAMPLE_RATE]))

    cg.add(var.set_conversion_time(compute_measurement_conversion_time(config)))
    return var
