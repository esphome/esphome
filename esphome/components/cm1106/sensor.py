"""CM1106 Sensor component for ESPHome."""

from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import sensor, uart
from esphome.components.const import CONF_AUTOMATIC_BASELINE_CALIBRATION
import esphome.config_validation as cv
from esphome.const import (
    CONF_BASELINE,
    CONF_CO2,
    CONF_CYCLE,
    CONF_ID,
    DEVICE_CLASS_CARBON_DIOXIDE,
    ICON_MOLECULE_CO2,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_MILLION,
)
from esphome.types import ConfigType

DEPENDENCIES = ["uart"]
CODEOWNERS = ["@andrewjswan"]


# true enables ABC with these defaults, false disables it, a mapping enables it with custom values
_ABC_SETTINGS_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_CYCLE, default=15): cv.int_range(min=1, max=90),
        cv.Optional(CONF_BASELINE, default=400): cv.int_range(min=400, max=10000),
    }
)


cm1106_ns = cg.esphome_ns.namespace("cm1106")
CM1106Component = cm1106_ns.class_(
    "CM1106Component", cg.PollingComponent, uart.UARTDevice
)


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CM1106Component),
            cv.Optional(CONF_CO2): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_MILLION,
                icon=ICON_MOLECULE_CO2,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_CARBON_DIOXIDE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_AUTOMATIC_BASELINE_CALIBRATION): cv.Any(
                cv.boolean, _ABC_SETTINGS_SCHEMA
            ),
        },
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "cm1106",
    baud_rate=9600,
    data_bits=8,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config: ConfigType) -> None:
    """Code generation entry point."""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_CO2, var.set_co2_sensor)

    if (abc := config.get(CONF_AUTOMATIC_BASELINE_CALIBRATION)) is not None:
        enabled = abc if isinstance(abc, bool) else True
        settings = _ABC_SETTINGS_SCHEMA({}) if isinstance(abc, bool) else abc
        cg.add(var.set_abc(enabled, settings[CONF_CYCLE], settings[CONF_BASELINE]))


CALIBRATION_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(CM1106Component),
    },
)


automation.register_apply_action(
    "cm1106.calibrate_zero",
    CALIBRATION_ACTION_SCHEMA,
    automation.ApplyCall("calibrate_zero(400)"),
)
for _name, _call in (
    ("cm1106.abc_enable", "abc_enable()"),
    ("cm1106.abc_disable", "abc_disable()"),
):
    automation.register_apply_action(
        _name, CALIBRATION_ACTION_SCHEMA, automation.ApplyCall(_call)
    )
