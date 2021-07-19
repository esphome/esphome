import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, uart
from esphome.const import (
    CONF_ID,
    CONF_PM_2_5,
    DEVICE_CLASS_EMPTY,
    STATE_CLASS_MEASUREMENT,
    UNIT_MICROGRAMS_PER_CUBIC_METER,
    ICON_CHEMICAL_WEAPON,
)

DEPENDENCIES = ["uart"]

pm1006_ns = cg.esphome_ns.namespace("pm1006")
PM1006Component = pm1006_ns.class_("PM1006Component", uart.UARTDevice, cg.Component)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PM1006Component),
            cv.Optional(CONF_PM_2_5): sensor.sensor_schema(
                UNIT_MICROGRAMS_PER_CUBIC_METER,
                ICON_CHEMICAL_WEAPON,
                0,
                DEVICE_CLASS_EMPTY,
                STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if CONF_PM_2_5 in config:
        sens = await sensor.new_sensor(config[CONF_PM_2_5])
        cg.add(var.set_pm_2_5_sensor(sens))
