import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_IIR_FILTER,
    CONF_OVERSAMPLING,
    CONF_PRESSURE,
    CONF_TEMPERATURE,
)
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

CODEOWNERS = ["@ademuri"]
DOMAIN = "bmp280_base"

bmp280_ns = cg.esphome_ns.namespace("bmp280_base")
BMP280Oversampling = bmp280_ns.enum("BMP280Oversampling")
OVERSAMPLING_OPTIONS = BMP280Oversampling.options(
    "NONE", "1X", "2X", "4X", "8X", "16X", prefix="BMP280_OVERSAMPLING_"
)

BMP280IIRFilter = bmp280_ns.enum("BMP280IIRFilter")
IIR_FILTER_OPTIONS = BMP280IIRFilter.options(
    "OFF", "2X", "4X", "8X", "16X", prefix="BMP280_IIR_FILTER_"
)

CONFIG_SCHEMA_BASE = cv.Schema(
    {
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
            kind=sensor.TEMPERATURE, accuracy_decimals=1
        ).extend(
            {
                cv.Optional(CONF_OVERSAMPLING, default="16X"): cv.enum(
                    OVERSAMPLING_OPTIONS, upper=True
                ),
            }
        ),
        cv.Optional(CONF_PRESSURE): sensor.sensor_schema(
            kind=sensor.PRESSURE, accuracy_decimals=1
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
).extend(cv.polling_component_schema("60s"))


async def to_code_base(config: ConfigType) -> MockObj:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    sensors = sensor.sub_sensors(config)
    if await sensors(CONF_TEMPERATURE, var.set_temperature_sensor):
        temperature_config = config[CONF_TEMPERATURE]
        cg.add(var.set_temperature_oversampling(temperature_config[CONF_OVERSAMPLING]))

    if await sensors(CONF_PRESSURE, var.set_pressure_sensor):
        pressure_config = config[CONF_PRESSURE]
        cg.add(var.set_pressure_oversampling(pressure_config[CONF_OVERSAMPLING]))

    cg.add(var.set_iir_filter(config[CONF_IIR_FILTER]))

    return var
