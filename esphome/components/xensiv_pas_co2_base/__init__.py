from esphome import pins
import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_CO2,
    CONF_ID,
    CONF_INTERRUPT_PIN,
    DEVICE_CLASS_CARBON_DIOXIDE,
    ICON_MOLECULE_CO2,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_MILLION,
)

CODEOWNERS = ["@michal-gora", "@ederjc", "@jaenrig-ifx"]

CONF_SENSOR_RATE = "sensor_rate"
CONF_OPERATION_MODE = "operation_mode"
CONF_PRESSURE_COMPENSATION = "pressure_compensation"
CONF_PRESSURE_COMPENSATION_SOURCE = "pressure_compensation_source"
CONF_POWER_PIN = "power_pin"


xensiv_pas_co2_ns = cg.esphome_ns.namespace("xensiv_pas_co2_base")

CONFIG_SCHEMA_BASE = cv.Schema(
    {
        cv.Required(CONF_CO2): sensor.sensor_schema(
            unit_of_measurement=UNIT_PARTS_PER_MILLION,
            icon=ICON_MOLECULE_CO2,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_CARBON_DIOXIDE,
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
        cv.Optional(CONF_PRESSURE_COMPENSATION): cv.pressure,
        cv.Optional(CONF_PRESSURE_COMPENSATION_SOURCE): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_POWER_PIN): pins.gpio_output_pin_schema,
    }
).extend(cv.COMPONENT_SCHEMA)

SENSOR_MAP = {
    CONF_CO2: "set_co2_sensor",
}


async def to_code_base(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    for key, funcName in SENSOR_MAP.items():
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(var, funcName)(sens))

    if CONF_PRESSURE_COMPENSATION in config:
        # cv.pressure returns value in Pascals (Pa), convert to hPa (1 bit = 1 hPa)
        pressure_hpa = int(config[CONF_PRESSURE_COMPENSATION]) // 100
        cg.add(var.set_pressure_compensation(pressure_hpa))

    if CONF_PRESSURE_COMPENSATION_SOURCE in config:
        sens = await cg.get_variable(config[CONF_PRESSURE_COMPENSATION_SOURCE])
        cg.add(var.set_pressure_compensation_source(sens))

    if CONF_INTERRUPT_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_INTERRUPT_PIN])
        cg.add(var.set_interrupt_pin(pin))

    if CONF_POWER_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_POWER_PIN])
        cg.add(var.set_power_pin(pin))

    if CONF_SENSOR_RATE in config:
        # Convert TimePeriod to total seconds
        cg.add(var.set_sensor_rate_value(config[CONF_SENSOR_RATE].total_seconds))

    if CONF_OPERATION_MODE in config:
        cg.add(var.set_operation_mode(config[CONF_OPERATION_MODE]))

    return var
