import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, uart
from esphome.const import (
    CONF_ID,
    CONF_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_AMPERE,
    UNIT_KILOWATT_HOURS,
    UNIT_VOLT,
    UNIT_WATT,
)

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

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BL0939),
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
            ),
            cv.Optional(CONF_ENERGY_2): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOWATT_HOURS,
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_ENERGY,
            ),
            cv.Optional(CONF_ENERGY_TOTAL): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOWATT_HOURS,
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_ENERGY,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if CONF_VOLTAGE in config:
        conf = config[CONF_VOLTAGE]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_voltage_sensor(sens))
    if CONF_CURRENT_1 in config:
        conf = config[CONF_CURRENT_1]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_current_sensor_1(sens))
    if CONF_CURRENT_2 in config:
        conf = config[CONF_CURRENT_2]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_current_sensor_2(sens))
    if CONF_ACTIVE_POWER_1 in config:
        conf = config[CONF_ACTIVE_POWER_1]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_power_sensor_1(sens))
    if CONF_ACTIVE_POWER_2 in config:
        conf = config[CONF_ACTIVE_POWER_2]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_power_sensor_2(sens))
    if CONF_ENERGY_1 in config:
        conf = config[CONF_ENERGY_1]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_energy_sensor_1(sens))
    if CONF_ENERGY_2 in config:
        conf = config[CONF_ENERGY_2]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_energy_sensor_2(sens))
    if CONF_ENERGY_TOTAL in config:
        conf = config[CONF_ENERGY_TOTAL]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_energy_sensor_sum(sens))
