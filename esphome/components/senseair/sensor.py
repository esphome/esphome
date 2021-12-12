import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.automation import maybe_simple_id
from esphome.components import sensor, uart
from esphome.const import (
    CONF_CO2,
    CONF_ID,
    ICON_MOLECULE_CO2,
    DEVICE_CLASS_CARBON_DIOXIDE,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_MILLION,
)

DEPENDENCIES = ["uart"]

senseair_ns = cg.esphome_ns.namespace("senseair")
SenseAirComponent = senseair_ns.class_(
    "SenseAirComponent", cg.PollingComponent, uart.UARTDevice
)
SenseAirBackgroundCalibrationAction = senseair_ns.class_(
    "SenseAirBackgroundCalibrationAction", automation.Action
)
SenseAirBackgroundCalibrationResultAction = senseair_ns.class_(
    "SenseAirBackgroundCalibrationResultAction", automation.Action
)
SenseAirABCEnableAction = senseair_ns.class_(
    "SenseAirABCEnableAction", automation.Action
)
SenseAirABCDisableAction = senseair_ns.class_(
    "SenseAirABCDisableAction", automation.Action
)
SenseAirABCGetPeriodAction = senseair_ns.class_(
    "SenseAirABCGetPeriodAction", automation.Action
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SenseAirComponent),
            cv.Required(CONF_CO2): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_MILLION,
                icon=ICON_MOLECULE_CO2,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_CARBON_DIOXIDE,
                state_class=STATE_CLASS_MEASUREMENT,
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

    if CONF_CO2 in config:
        sens = await sensor.new_sensor(config[CONF_CO2])
        cg.add(var.set_co2_sensor(sens))


CALIBRATION_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(SenseAirComponent),
    }
)


@automation.register_action(
    "senseair.background_calibration",
    SenseAirBackgroundCalibrationAction,
    CALIBRATION_ACTION_SCHEMA,
)
@automation.register_action(
    "senseair.background_calibration_result",
    SenseAirBackgroundCalibrationResultAction,
    CALIBRATION_ACTION_SCHEMA,
)
@automation.register_action(
    "senseair.abc_enable", SenseAirABCEnableAction, CALIBRATION_ACTION_SCHEMA
)
@automation.register_action(
    "senseair.abc_disable", SenseAirABCDisableAction, CALIBRATION_ACTION_SCHEMA
)
@automation.register_action(
    "senseair.abc_get_period", SenseAirABCGetPeriodAction, CALIBRATION_ACTION_SCHEMA
)
async def senseair_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)
