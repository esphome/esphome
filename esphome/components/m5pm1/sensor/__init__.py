import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ENTITY_CATEGORY,
    CONF_NUMBER,
    CONF_PIN,
    CONF_TYPE,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_THERMOMETER,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_VOLT,
)
from esphome.types import ConfigType

from .. import (
    BASE_SCHEMA,
    CONF_M5PM1_ID,
    M5PM1_GPIO_INPUT_PIN_SCHEMA,
    M5PM1ADCChannel,
    m5pm1_ns,
)

DEPENDENCIES = ["m5pm1"]

M5PM1ADCSensor = m5pm1_ns.class_("M5PM1ADCSensor", sensor.Sensor, cg.PollingComponent)
M5PM1TemperatureSensor = m5pm1_ns.class_(
    "M5PM1TemperatureSensor", sensor.Sensor, cg.PollingComponent
)
M5PM1VoltageSensor = m5pm1_ns.class_(
    "M5PM1VoltageSensor", sensor.Sensor, cg.PollingComponent
)
M5PM1VoltageType = m5pm1_ns.enum("M5PM1VoltageType", is_class=True)

M5PM1_ADC_PIN_TO_CHANNEL = {
    1: M5PM1ADCChannel.M5PM1_ADC_CHANNEL_1,
    2: M5PM1ADCChannel.M5PM1_ADC_CHANNEL_2,
}

VOLTAGE_TYPES = {
    "battery_voltage": M5PM1VoltageType.M5PM1_VOLTAGE_TYPE_BATTERY,
    "vin_voltage": M5PM1VoltageType.M5PM1_VOLTAGE_TYPE_VIN,
    "5vout_voltage": M5PM1VoltageType.M5PM1_VOLTAGE_TYPE_5VOUT,
    "vref_voltage": M5PM1VoltageType.M5PM1_VOLTAGE_TYPE_VREF,
}

TEMPERATURE_SCHEMA = (
    sensor.sensor_schema(
        M5PM1TemperatureSensor,
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
        icon=ICON_THERMOMETER,
    )
    .extend(BASE_SCHEMA)
    .extend(cv.polling_component_schema("60s"))
)


def validate_adc_pin(pin: ConfigType) -> ConfigType:
    if pin[CONF_NUMBER] not in (1, 2):
        raise cv.Invalid(
            "M5PM1 ADC only supports GPIO1 and GPIO2",
            path=[CONF_NUMBER],
        )
    return pin


M5PM1_ADC_PIN_SCHEMA = cv.All(
    M5PM1_GPIO_INPUT_PIN_SCHEMA,
    validate_adc_pin,
)

ADC_SCHEMA = (
    sensor.sensor_schema(M5PM1ADCSensor)
    .extend({cv.Required(CONF_PIN): M5PM1_ADC_PIN_SCHEMA})
    .extend(BASE_SCHEMA)
    .extend(cv.polling_component_schema("60s"))
)

VOLTAGE_SCHEMA = (
    sensor.sensor_schema(
        M5PM1VoltageSensor,
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(BASE_SCHEMA)
    .extend(cv.polling_component_schema("60s"))
)

VREF_VOLTAGE_SCHEMA = VOLTAGE_SCHEMA.extend(
    {
        cv.Optional(
            CONF_ENTITY_CATEGORY, default=ENTITY_CATEGORY_DIAGNOSTIC
        ): cv.entity_category
    }
)

CONFIG_SCHEMA = cv.typed_schema(
    {
        "temperature": TEMPERATURE_SCHEMA,
        "adc": ADC_SCHEMA,
        "battery_voltage": VOLTAGE_SCHEMA,
        "vin_voltage": VOLTAGE_SCHEMA,
        "5vout_voltage": VOLTAGE_SCHEMA,
        "vref_voltage": VREF_VOLTAGE_SCHEMA,
    },
    key=CONF_TYPE,
)


async def to_code(config: ConfigType) -> None:
    sensor_type = config[CONF_TYPE]
    if sensor_type == "adc":
        pin = await cg.gpio_pin_expression(config[CONF_PIN])
        var = await sensor.new_sensor(config)
        cg.add(var.set_pin(pin))
        cg.add(var.set_channel(M5PM1_ADC_PIN_TO_CHANNEL[config[CONF_PIN][CONF_NUMBER]]))
    elif sensor_type in VOLTAGE_TYPES:
        var = await sensor.new_sensor(config, VOLTAGE_TYPES[sensor_type])
    else:
        var = await sensor.new_sensor(config)

    await cg.register_parented(var, config[CONF_M5PM1_ID])
    await cg.register_component(var, config)
