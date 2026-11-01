import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_DATA_RATE,
    CONF_ID,
    CONF_IIR_FILTER,
    CONF_OVERSAMPLING,
    CONF_PRESSURE,
    CONF_TEMPERATURE,
    CONF_UPDATE_INTERVAL,
    DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_HECTOPASCAL,
)
from esphome.cpp_generator import MockObj, MockObjClass
from esphome.types import ConfigType

CODEOWNERS = ["@martgras", "@latonita"]
DOMAIN = "bmp3xx_base"

bmp3xx_ns = cg.esphome_ns.namespace("bmp3xx_base")
Oversampling = bmp3xx_ns.enum("Oversampling")
OVERSAMPLING_OPTIONS = {
    "NONE": Oversampling.OVERSAMPLING_NONE,
    "2X": Oversampling.OVERSAMPLING_X2,
    "4X": Oversampling.OVERSAMPLING_X4,
    "8X": Oversampling.OVERSAMPLING_X8,
    "16X": Oversampling.OVERSAMPLING_X16,
    "32X": Oversampling.OVERSAMPLING_X32,
}

IIRFilter = bmp3xx_ns.enum("IIRFilter")
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

OutputDataRate = bmp3xx_ns.enum("OutputDataRate")
DATA_RATE_OPTIONS = {
    "200HZ": OutputDataRate.ODR_200_HZ,
    "100HZ": OutputDataRate.ODR_100_HZ,
    "50HZ": OutputDataRate.ODR_50_HZ,
    "25HZ": OutputDataRate.ODR_25_HZ,
    "12.5HZ": OutputDataRate.ODR_12_5_HZ,
    "6.25HZ": OutputDataRate.ODR_6_25_HZ,
    "3.1HZ": OutputDataRate.ODR_3_1_HZ,
    "1.5HZ": OutputDataRate.ODR_1_5_HZ,
    "0.78HZ": OutputDataRate.ODR_0_78_HZ,
    "0.39HZ": OutputDataRate.ODR_0_39_HZ,
    "0.2HZ": OutputDataRate.ODR_0_2_HZ,
    "0.1HZ": OutputDataRate.ODR_0_1_HZ,
    "0.05HZ": OutputDataRate.ODR_0_05_HZ,
    "0.02HZ": OutputDataRate.ODR_0_02_HZ,
    "0.01HZ": OutputDataRate.ODR_0_01_HZ,
    "0.006HZ": OutputDataRate.ODR_0_006_HZ,
    "0.003HZ": OutputDataRate.ODR_0_003_HZ,
    "0.001HZ": OutputDataRate.ODR_0_001_HZ,
}


def _validate_data_rate(config: ConfigType) -> ConfigType:
    """One measurement must fit in the data rate period (BMP390 datasheet 3.9.2, as in update())."""
    if (data_rate := config.get(CONF_DATA_RATE)) is None:
        return config
    # DATA_RATE_OPTIONS is in register order and each step doubles the 5 ms period
    period_ms = 5 * 2 ** list(DATA_RATE_OPTIONS).index(data_rate)
    oversampling = list(OVERSAMPLING_OPTIONS)
    meas_us = 234
    if CONF_PRESSURE in config:
        osr_p = oversampling.index(config[CONF_PRESSURE][CONF_OVERSAMPLING])
        meas_us += 392 + 2020 * 2**osr_p
    osr_t = 0
    if CONF_TEMPERATURE in config:
        osr_t = oversampling.index(config[CONF_TEMPERATURE][CONF_OVERSAMPLING])
    meas_us += 163 + 2020 * 2**osr_t
    if meas_us > period_ms * 1000:
        raise cv.Invalid(
            f"One measurement takes {meas_us / 1000:.1f} ms with these oversampling "
            f"settings, longer than the {period_ms} ms data rate period. "
            "Lower the oversampling or choose a slower data_rate.",
            path=[CONF_DATA_RATE],
        )
    if config[CONF_UPDATE_INTERVAL].total_milliseconds < period_ms:
        raise cv.Invalid(
            f"update_interval must be at least the {period_ms} ms data rate period.",
            path=[CONF_UPDATE_INTERVAL],
        )
    return config


CONFIG_SCHEMA_BASE = cv.Schema(
    {
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ).extend(
            {
                cv.Optional(CONF_OVERSAMPLING, default="2X"): cv.enum(
                    OVERSAMPLING_OPTIONS, upper=True
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
            }
        ),
        cv.Optional(CONF_IIR_FILTER, default="OFF"): cv.enum(
            IIR_FILTER_OPTIONS, upper=True
        ),
        cv.Optional(CONF_DATA_RATE): cv.enum(DATA_RATE_OPTIONS, upper=True),
    }
).extend(cv.polling_component_schema("60s"))


def bmp3xx_schema(bus_schema: cv.Schema, component: MockObjClass) -> cv.All:
    """The sensor schema for one bus, with the data_rate checks."""
    return cv.All(
        CONFIG_SCHEMA_BASE.extend(bus_schema).extend(
            {cv.GenerateID(): cv.declare_id(component)}
        ),
        _validate_data_rate,
    )


async def to_code_base(config: ConfigType) -> MockObj:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_iir_filter_config(config[CONF_IIR_FILTER]))
    cg.add_optional(var.set_output_data_rate_config, config.get(CONF_DATA_RATE))
    sensors = sensor.sub_sensors(config)
    if await sensors(CONF_TEMPERATURE, var.set_temperature_sensor):
        temperature_config = config[CONF_TEMPERATURE]
        cg.add(
            var.set_temperature_oversampling_config(
                temperature_config[CONF_OVERSAMPLING]
            )
        )

    if await sensors(CONF_PRESSURE, var.set_pressure_sensor):
        pressure_config = config[CONF_PRESSURE]
        cg.add(var.set_pressure_oversampling_config(pressure_config[CONF_OVERSAMPLING]))

    return var
