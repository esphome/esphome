import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MAX_LEVEL,
    CONF_MIN_LEVEL,
    ICON_EMPTY,
    UNIT_EMPTY,
)
from esphome.types import ConfigType

CODEOWNERS = ["@JakeLC15"]
DEPENDENCIES = ["uart"]

ds1603l_ns = cg.esphome_ns.namespace("ds1603l")
DS1603L = ds1603l_ns.class_("DS1603L", sensor.Sensor, cg.Component, uart.UARTDevice)

CONF_LIQUID_LEVEL = "liquid_level"
CONF_LIQUID_VOLUME = "liquid_volume"
CONF_PERCENTAGE = "percentage"
CONF_MIN_VOLUME = "min_volume"
CONF_MAX_VOLUME = "max_volume"

CONFIG_SCHEMA = uart.UART_DEVICE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(DS1603L),
        cv.Optional(CONF_LIQUID_LEVEL): sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            icon=ICON_EMPTY,
            accuracy_decimals=0,
            state_class="measurement",
        ),
        cv.Optional(CONF_LIQUID_VOLUME): sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            icon=ICON_EMPTY,
            accuracy_decimals=0,
            state_class="measurement",
        ),
        cv.Optional(CONF_PERCENTAGE): sensor.sensor_schema(
            unit_of_measurement="%",
            icon=ICON_EMPTY,
            accuracy_decimals=0,
            state_class="measurement",
        ),
        cv.Optional(CONF_MIN_VOLUME, default=0.0): cv.float_,
        cv.Optional(CONF_MAX_VOLUME, default=1000.0): cv.float_,
        cv.Optional(CONF_MIN_LEVEL, default=0.0): cv.float_,
        cv.Optional(CONF_MAX_LEVEL, default=1000.0): cv.float_,
    }
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if liquid_level := config.get(CONF_LIQUID_LEVEL):
        sens = await sensor.new_sensor(liquid_level)
        cg.add(var.set_liquid_level_sensor(sens))

    if liquid_volume := config.get(CONF_LIQUID_VOLUME):
        sens = await sensor.new_sensor(liquid_volume)
        cg.add(var.set_liquid_volume_sensor(sens))

    if percentage := config.get(CONF_PERCENTAGE):
        sens = await sensor.new_sensor(percentage)
        cg.add(var.set_percentage_sensor(sens))

    for key in (CONF_MIN_VOLUME, CONF_MAX_VOLUME, CONF_MIN_LEVEL, CONF_MAX_LEVEL):
        if conf_var := config.get(key):
            cg.add(getattr(var, f"set_{key}")(conf_var))
