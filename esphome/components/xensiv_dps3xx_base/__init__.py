from esphome import pins
import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_INTERRUPT_PIN,
    CONF_PRESSURE,
    CONF_TEMPERATURE,
    DEVICE_CLASS_ATMOSPHERIC_PRESSURE,
    ICON_GAUGE,
    ICON_THERMOMETER,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_HECTOPASCAL,
)

CODEOWNERS = ["@michal-gora", "@ederjc", "@jaenrig-ifx"]

CONF_SENSOR_RATE = "sensor_rate"
CONF_OPERATION_MODE = "operation_mode"

xensiv_dps3xx_ns = cg.esphome_ns.namespace("xensiv_dps3xx_base")

CONFIG_SCHEMA_BASE = cv.Schema(
    {
        cv.Required(CONF_PRESSURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_HECTOPASCAL,
            icon=ICON_GAUGE,
            accuracy_decimals=5,
            device_class=DEVICE_CLASS_ATMOSPHERIC_PRESSURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Required(CONF_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon=ICON_THERMOMETER,
            accuracy_decimals=2,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Required(CONF_INTERRUPT_PIN): pins.internal_gpio_input_pin_schema,
        cv.Optional(CONF_SENSOR_RATE, default="60s"): cv.All(
            cv.positive_time_period_seconds,
            cv.Range(min=cv.TimePeriod(seconds=5), max=cv.TimePeriod(seconds=4095)),
        ),
        cv.Optional(CONF_OPERATION_MODE, default="continuous"): cv.enum(
            {
                "single_shot": 0,
                "continuous": 1,
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)

SENSOR_MAP = {
    CONF_PRESSURE: "set_pressure_sensor",
    CONF_TEMPERATURE: "set_temperature_sensor",
}


async def to_code_base(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    for key, funcName in SENSOR_MAP.items():
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(var, funcName)(sens))

    if CONF_INTERRUPT_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_INTERRUPT_PIN])
        cg.add(var.set_interrupt_pin(pin))

    if CONF_SENSOR_RATE in config:
        # Convert TimePeriod to total seconds
        cg.add(var.set_sensor_rate_value(config[CONF_SENSOR_RATE].total_seconds))

    if CONF_OPERATION_MODE in config:
        cg.add(var.set_operation_mode(config[CONF_OPERATION_MODE]))
    return var
