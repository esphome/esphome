import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, sensor, text_sensor
from esphome.const import (
    CONF_ID,
    CONF_STATE,
    DEVICE_CLASS_VOLTAGE,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    ICON_FLASH,
    UNIT_VOLT,
    ICON_THERMOMETER,
    UNIT_WATT,
    UNIT_CELSIUS,
    CONF_TEMPERATURE,
)

CODEOWNERS = ["@Mat931"]
MULTI_CONF = True
DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor", "text_sensor"]

sun_gtil2_ns = cg.esphome_ns.namespace("sun_gtil2")

SunGTIL2 = sun_gtil2_ns.class_("SunGTIL2", cg.Component, uart.UARTDevice)


CONF_AC_VOLTAGE = "ac_voltage"
CONF_DC_VOLTAGE = "dc_voltage"
CONF_AC_POWER = "ac_power"
CONF_DC_POWER = "dc_power"
CONF_LIMITER_POWER = "limiter_power"
CONF_SERIAL_NUMBER = "serial_number"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SunGTIL2),
        cv.Optional(CONF_AC_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            icon=ICON_FLASH,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_VOLTAGE,
        ),
        cv.Optional(CONF_DC_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            icon=ICON_FLASH,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_VOLTAGE,
        ),
        cv.Optional(CONF_AC_POWER): sensor.sensor_schema(
            unit_of_measurement=UNIT_WATT,
            icon=ICON_FLASH,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_POWER,
        ),
        cv.Optional(CONF_DC_POWER): sensor.sensor_schema(
            unit_of_measurement=UNIT_WATT,
            icon=ICON_FLASH,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_POWER,
        ),
        cv.Optional(CONF_LIMITER_POWER): sensor.sensor_schema(
            unit_of_measurement=UNIT_WATT,
            icon=ICON_FLASH,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_POWER,
        ),
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon=ICON_THERMOMETER,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
        ),
        cv.Optional(CONF_STATE): text_sensor.text_sensor_schema(text_sensor.TextSensor),
        cv.Optional(CONF_SERIAL_NUMBER): text_sensor.text_sensor_schema(
            text_sensor.TextSensor
        ),
    }
).extend(uart.UART_DEVICE_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if CONF_AC_VOLTAGE in config:
        sens = await sensor.new_sensor(config[CONF_AC_VOLTAGE])
        cg.add(var.set_ac_voltage(sens))
    if CONF_DC_VOLTAGE in config:
        sens = await sensor.new_sensor(config[CONF_DC_VOLTAGE])
        cg.add(var.set_dc_voltage(sens))
    if CONF_AC_POWER in config:
        sens = await sensor.new_sensor(config[CONF_AC_POWER])
        cg.add(var.set_ac_power(sens))
    if CONF_DC_POWER in config:
        sens = await sensor.new_sensor(config[CONF_DC_POWER])
        cg.add(var.set_dc_power(sens))
    if CONF_LIMITER_POWER in config:
        sens = await sensor.new_sensor(config[CONF_LIMITER_POWER])
        cg.add(var.set_limiter_power(sens))
    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature(sens))
    if CONF_STATE in config:
        sens = await text_sensor.new_text_sensor(config[CONF_STATE])
        cg.add(var.set_state(sens))
    if CONF_SERIAL_NUMBER in config:
        sens = await text_sensor.new_text_sensor(config[CONF_SERIAL_NUMBER])
        cg.add(var.set_serial_number(sens))
