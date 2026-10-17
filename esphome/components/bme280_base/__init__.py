import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_HUMIDITY,
    CONF_ID,
    CONF_IIR_FILTER,
    CONF_OVERSAMPLING,
    CONF_PRESSURE,
    CONF_TEMPERATURE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_HECTOPASCAL,
    UNIT_PERCENT,
)
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

CODEOWNERS = ["@esphome/core"]
DOMAIN = "bme280_base"

bme280_ns = cg.esphome_ns.namespace("bme280_base")
BME280Oversampling = bme280_ns.enum("BME280Oversampling")
OVERSAMPLING_OPTIONS = BME280Oversampling.options(
    "NONE", "1X", "2X", "4X", "8X", "16X", prefix="BME280_OVERSAMPLING_"
)

BME280IIRFilter = bme280_ns.enum("BME280IIRFilter")
IIR_FILTER_OPTIONS = BME280IIRFilter.options(
    "OFF", "2X", "4X", "8X", "16X", prefix="BME280_IIR_FILTER_"
)

CONFIG_SCHEMA_BASE = cv.Schema(
    {
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ).extend(
            {
                cv.Optional(CONF_OVERSAMPLING, default="16X"): cv.enum(
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
        cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_HUMIDITY,
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
    }
).extend(cv.polling_component_schema("60s"))


async def to_code_base(config: ConfigType) -> MockObj:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    sensors = sensor.sub_sensors(config)
    if await sensors(CONF_TEMPERATURE, var.set_temperature_sensor):
        oversampling = config[CONF_TEMPERATURE][CONF_OVERSAMPLING]
        cg.add(var.set_temperature_oversampling(oversampling))
    if await sensors(CONF_PRESSURE, var.set_pressure_sensor):
        cg.add(var.set_pressure_oversampling(config[CONF_PRESSURE][CONF_OVERSAMPLING]))
    if await sensors(CONF_HUMIDITY, var.set_humidity_sensor):
        cg.add(var.set_humidity_oversampling(config[CONF_HUMIDITY][CONF_OVERSAMPLING]))

    cg.add(var.set_iir_filter(config[CONF_IIR_FILTER]))

    return var
