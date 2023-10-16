import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_HUMIDITY,
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


bme280_ns = cg.esphome_ns.namespace("bme280")
BME280Oversampling = bme280_ns.enum("BME280Oversampling")
OVERSAMPLING_OPTIONS = {
    "NONE": BME280Oversampling.BME280_OVERSAMPLING_NONE,
    "1X": BME280Oversampling.BME280_OVERSAMPLING_1X,
    "2X": BME280Oversampling.BME280_OVERSAMPLING_2X,
    "4X": BME280Oversampling.BME280_OVERSAMPLING_4X,
    "8X": BME280Oversampling.BME280_OVERSAMPLING_8X,
    "16X": BME280Oversampling.BME280_OVERSAMPLING_16X,
}

BME280IIRFilter = bme280_ns.enum("BME280IIRFilter")
IIR_FILTER_OPTIONS = {
    "OFF": BME280IIRFilter.BME280_IIR_FILTER_OFF,
    "2X": BME280IIRFilter.BME280_IIR_FILTER_2X,
    "4X": BME280IIRFilter.BME280_IIR_FILTER_4X,
    "8X": BME280IIRFilter.BME280_IIR_FILTER_8X,
    "16X": BME280IIRFilter.BME280_IIR_FILTER_16X,
}

BME280I2CComponent = bme280_ns.class_(
    "BME280I2CComponent", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA_BASE = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BME280I2CComponent),
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


async def to_code(conf):
    pass
