import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_INTERNAL_TEMPERATURE,
    CONF_MODE,
    CONF_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_KILOWATT_HOURS,
    UNIT_VOLT,
    UNIT_WATT,
)
from esphome.types import ConfigType

DEPENDENCIES = ["uart"]

CONF_CURRENT_1 = "current_1"
CONF_CURRENT_2 = "current_2"
CONF_ACTIVE_POWER_1 = "active_power_1"
CONF_ACTIVE_POWER_2 = "active_power_2"
CONF_ENERGY_1 = "energy_1"
CONF_ENERGY_2 = "energy_2"
CONF_ENERGY_TOTAL = "energy_total"

bl0939_ns = cg.esphome_ns.namespace("bl0939")
BL0939 = bl0939_ns.class_("BL0939", cg.PollingComponent, uart.UARTDevice)
BL0939Mode = bl0939_ns.enum("BL0939Mode", is_class=True)
MODE_OPTIONS = BL0939Mode.options(
    "direct", "current_transformer", prefix="BL0939_MODE_"
)
# Must match the mode_ member initializer in bl0939.h; set_mode() is skipped
# when the config already equals it.
DEFAULT_MODE = "direct"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BL0939),
            cv.Optional(CONF_MODE, default=DEFAULT_MODE): cv.enum(
                MODE_OPTIONS, lower=True
            ),
            cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_CURRENT_1): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_CURRENT,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_CURRENT_2): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_CURRENT,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_ACTIVE_POWER_1): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_ACTIVE_POWER_2): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_ENERGY_1): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOWATT_HOURS,
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_ENERGY_2): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOWATT_HOURS,
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_ENERGY_TOTAL): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOWATT_HOURS,
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_INTERNAL_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if (mode := config[CONF_MODE]) != DEFAULT_MODE:
        cg.add(var.set_mode(mode))

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_VOLTAGE, var.set_voltage_sensor)
    await sensors(CONF_CURRENT_1, var.set_current_sensor_1)
    await sensors(CONF_CURRENT_2, var.set_current_sensor_2)
    await sensors(CONF_ACTIVE_POWER_1, var.set_power_sensor_1)
    await sensors(CONF_ACTIVE_POWER_2, var.set_power_sensor_2)
    await sensors(CONF_ENERGY_1, var.set_energy_sensor_1)
    await sensors(CONF_ENERGY_2, var.set_energy_sensor_2)
    await sensors(CONF_ENERGY_TOTAL, var.set_energy_sensor_sum)
    await sensors(CONF_INTERNAL_TEMPERATURE, var.set_internal_temperature_sensor)
