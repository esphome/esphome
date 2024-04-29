from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, sensor, spi
from esphome.const import (
    CONF_MAINS_FILTER,
    CONF_TYPE,
    CONF_UPDATE_INTERVAL,
    DEVICE_CLASS_PROBLEM,
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)

CONF_DATA_READY_PIN = "data_ready_pin"
CONF_HAS_FAULT = "has_fault"
CONF_SAMPLES_PER_VALUE = "samples_per_value"

max31856_ns = cg.esphome_ns.namespace("max31856")
MAX31856Sensor = max31856_ns.class_(
    "MAX31856Sensor", sensor.Sensor, cg.PollingComponent, spi.SPIDevice
)

MAX31865ConfigFilter = max31856_ns.enum("MAX31856ConfigFilter")
FILTER = {
    "50HZ": MAX31865ConfigFilter.FILTER_50HZ,
    "60HZ": MAX31865ConfigFilter.FILTER_60HZ,
}

MAX31856ThermocoupleType = max31856_ns.enum("MAX31856ThermocoupleType")
TYPE = {
    "B": MAX31856ThermocoupleType.MAX31856_TCTYPE_B,
    "E": MAX31856ThermocoupleType.MAX31856_TCTYPE_E,
    "J": MAX31856ThermocoupleType.MAX31856_TCTYPE_J,
    "K": MAX31856ThermocoupleType.MAX31856_TCTYPE_K,
    "N": MAX31856ThermocoupleType.MAX31856_TCTYPE_N,
    "R": MAX31856ThermocoupleType.MAX31856_TCTYPE_R,
    "S": MAX31856ThermocoupleType.MAX31856_TCTYPE_S,
    "T": MAX31856ThermocoupleType.MAX31856_TCTYPE_T,
}

MAX31856SamplesPerValue = max31856_ns.enum("MAX31856SamplesPerValue")
SAMPLES_PER_VALUE = {
    1: MAX31856SamplesPerValue.AVE_SAMPLES_1,
    2: MAX31856SamplesPerValue.AVE_SAMPLES_2,
    4: MAX31856SamplesPerValue.AVE_SAMPLES_4,
    8: MAX31856SamplesPerValue.AVE_SAMPLES_8,
    16: MAX31856SamplesPerValue.AVE_SAMPLES_16,
}

exclusive_err_msg = "Use only one of data_ready_pin or update_interval"

# Unfortunately, Exclusive doesn't pass a default through to the base Optional so we have to set it manually
exclusive_update_interval_with_default = cv.Exclusive(
    CONF_UPDATE_INTERVAL,
    "mode",
    exclusive_err_msg,
    "update_interval will enable polling mode",
)
exclusive_update_interval_with_default.default = lambda: "60s"

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        MAX31856Sensor,
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.Optional(CONF_HAS_FAULT): binary_sensor.binary_sensor_schema(
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                device_class=DEVICE_CLASS_PROBLEM,
            ),
            cv.Optional(CONF_MAINS_FILTER, default="60HZ"): cv.enum(
                FILTER, upper=True, space=""
            ),
            cv.Optional(CONF_TYPE, default="K"): cv.enum(TYPE, upper=True, space=""),
            cv.Optional(CONF_SAMPLES_PER_VALUE, default=1): cv.enum(
                SAMPLES_PER_VALUE, int=True, space=""
            ),
            cv.Exclusive(
                CONF_DATA_READY_PIN,
                "mode",
                exclusive_err_msg,
                "data_ready_pin will enable autoconversion mode",
            ): pins.gpio_input_pullup_pin_schema,
            exclusive_update_interval_with_default: cv.update_interval,
        }
    )
    .extend(spi.spi_device_schema())
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)
    cg.add(var.set_filter(config[CONF_MAINS_FILTER]))
    cg.add(var.set_samples_per_value(config[CONF_SAMPLES_PER_VALUE]))
    cg.add(var.set_type(config[CONF_TYPE]))
    data_ready_config = config.get(CONF_DATA_READY_PIN)
    if data_ready_config is not None:
        pin = await cg.gpio_pin_expression(data_ready_config)
        cg.add(var.set_data_ready_pin(pin))
    has_fault = config.get(CONF_HAS_FAULT)
    if has_fault is not None:
        sens = await binary_sensor.new_binary_sensor(has_fault)
        cg.add(var.set_has_fault_binary_sensor(sens))
