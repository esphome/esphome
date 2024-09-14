import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
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

CODEOWNERS = ["@ademuri"]

bmp280_ns = cg.esphome_ns.namespace("bmp280_base")
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
        cv.Optional(CONF_IIR_FILTER, default="OFF"): cv.enum(
            IIR_FILTER_OPTIONS, upper=True
        ),
    }
).extend(cv.polling_component_schema("60s"))


async def to_code_base(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if temperature_config := config.get(CONF_TEMPERATURE):
        sens = await sensor.new_sensor(temperature_config)
        cg.add(var.set_temperature_sensor(sens))
        cg.add(var.set_temperature_oversampling(temperature_config[CONF_OVERSAMPLING]))

    if pressure_config := config.get(CONF_PRESSURE):
        sens = await sensor.new_sensor(pressure_config)
        cg.add(var.set_pressure_sensor(sens))
        cg.add(var.set_pressure_oversampling(pressure_config[CONF_OVERSAMPLING]))

    cg.add(var.set_iir_filter(config[CONF_IIR_FILTER]))

    return var
