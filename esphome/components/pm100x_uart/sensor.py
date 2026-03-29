import esphome.codegen as cg
from esphome.components import pm100x, sensor, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MODEL,
    CONF_PM_1_0,
    CONF_PM_2_5,
    CONF_PM_10_0,
    CONF_STARTUP_DELAY,
    DEVICE_CLASS_PM1,
    DEVICE_CLASS_PM10,
    DEVICE_CLASS_PM25,
    ICON_BLUR,
    STATE_CLASS_MEASUREMENT,
    UNIT_MICROGRAMS_PER_CUBIC_METER,
)

AUTO_LOAD = ["pm100x"]
CODEOWNERS = ["@tuct", "@habbie"]
DEPENDENCIES = ["uart"]

pm100x_uart_ns = cg.esphome_ns.namespace("pm100x_uart")
PM100XComponentUART = pm100x_uart_ns.class_(
    "PM100XComponentUART", pm100x.PM100XComponent, uart.UARTDevice
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PM100XComponentUART),
            cv.Optional(CONF_MODEL, default="pm1003"): cv.one_of(
                "pm1003", "pm1006", "pm1006k", lower=True
            ),
            cv.Optional(CONF_PM_2_5): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_BLUR,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_PM25,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PM_1_0): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_BLUR,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_PM1,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PM_10_0): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_BLUR,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_PM10,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_STARTUP_DELAY, default="15s"): cv.positive_time_period,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.polling_component_schema("60s"))
)


FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "pm100x_uart", baud_rate=9600, require_rx=True, require_tx=True
)


async def to_code(config):
    cg.add_global(
        cg.RawStatement('#include "esphome/components/pm100x_uart/pm100x_uart.h"')
    )

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_model(pm100x.MODEL_OPTIONS[config[CONF_MODEL]]))

    if CONF_PM_2_5 in config:
        sens = await sensor.new_sensor(config[CONF_PM_2_5])
        cg.add(var.set_pm_2_5_sensor(sens))

    if CONF_PM_1_0 in config:
        sens = await sensor.new_sensor(config[CONF_PM_1_0])
        cg.add(var.set_pm_1_0_sensor(sens))

    if CONF_PM_10_0 in config:
        sens = await sensor.new_sensor(config[CONF_PM_10_0])
        cg.add(var.set_pm_10_0_sensor(sens))

    cg.add(var.set_startup_delay(config[CONF_STARTUP_DELAY].total_milliseconds))
