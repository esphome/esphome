"""CM1106 Sensor component for ESPHome."""

from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_CO2,
    CONF_ID,
    DEVICE_CLASS_CARBON_DIOXIDE,
    ICON_MOLECULE_CO2,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_MILLION,
)

DEPENDENCIES = ["uart"]
CODEOWNERS = ["@andrewjswan"]

cm1106_ns = cg.esphome_ns.namespace("cm1106")
CM1106Component = cm1106_ns.class_(
    "CM1106Component", cg.PollingComponent, uart.UARTDevice
)
CM1106CalibrateZeroAction = cm1106_ns.class_(
    "CM1106CalibrateZeroAction",
    automation.Action,
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
        },
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config) -> None:
    """Code generation entry point."""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    if co2_config := config.get(CONF_CO2):
        sens = await sensor.new_sensor(co2_config)
        cg.add(var.set_co2_sensor(sens))


CALIBRATION_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(CM1106Component),
    },
)


@automation.register_action(
    "cm1106.calibrate_zero",
    CM1106CalibrateZeroAction,
    CALIBRATION_ACTION_SCHEMA,
)
async def cm1106_calibration_to_code(config, action_id, template_arg, args) -> None:
    """Service code generation entry point."""
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)
