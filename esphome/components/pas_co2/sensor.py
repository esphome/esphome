import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.automation import maybe_simple_id
from esphome.components import sensor, i2c
from esphome.const import (
    CONF_CO2,
    CONF_ID,
    ICON_MOLECULE_CO2,
    DEVICE_CLASS_CARBON_DIOXIDE,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_MILLION,
)

CODEOWNERS = ["@martgras"]
DEPENDENCIES = ["i2c"]

pas_co2_ns = cg.esphome_ns.namespace("pas_co2")
PasCo2Component = pas_co2_ns.class_(
    "PasCo2Component", cg.PollingComponent, i2c.I2CDevice
)
PasCo2BackgroundCalibrationAction = pas_co2_ns.class_(
    "PerformForcedCalibrationAction", automation.Action
)

PasCo2ABCEnableAction = pas_co2_ns.class_("ABCEnableAction", automation.Action)
PasCo2ABCDisableAction = pas_co2_ns.class_("ABCDisableAction", automation.Action)


CONF_AMBIENT_PRESSURE_COMPENSATION = "ambient_pressure_compensation"
CONF_AMBIENT_PRESSURE_COMPENSATION_SOURCE = "ambient_pressure_compensation_source"
CONF_AUTOMATIC_SELF_CALIBRATION = "automatic_self_calibration"
CONF_CALIBRATION_OFFSET = "calibration_offset"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PasCo2Component),
            cv.Required(CONF_CO2): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_MILLION,
                icon=ICON_MOLECULE_CO2,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_CARBON_DIOXIDE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_CALIBRATION_OFFSET): cv.int_range(350, 900),
            cv.Optional(CONF_AMBIENT_PRESSURE_COMPENSATION): cv.pressure,
            cv.Optional(CONF_AMBIENT_PRESSURE_COMPENSATION_SOURCE): cv.use_id(
                sensor.Sensor
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x28))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_CO2 in config:
        sens = await sensor.new_sensor(config[CONF_CO2])
        cg.add(var.set_co2_sensor(sens))
    if CONF_AMBIENT_PRESSURE_COMPENSATION in config:
        cg.add(
            var.set_ambient_pressure_compensation(
                config[CONF_AMBIENT_PRESSURE_COMPENSATION]
            )
        )
    if CONF_AMBIENT_PRESSURE_COMPENSATION_SOURCE in config:
        sens = await cg.get_variable(config[CONF_AMBIENT_PRESSURE_COMPENSATION_SOURCE])
        cg.add(var.set_ambient_pressure_source(sens))
    if CONF_CALIBRATION_OFFSET in config:
        cg.add(var.set_calibration_offset(config[CONF_CALIBRATION_OFFSET]))
    if CONF_AUTOMATIC_SELF_CALIBRATION in config:
        cg.add(var.set_abc_enable(config[CONF_AUTOMATIC_SELF_CALIBRATION]))


CALIBRATION_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(PasCo2Component),
    }
)


@automation.register_action(
    "pas_co2.perform_forced_calibration",
    PasCo2BackgroundCalibrationAction,
    CALIBRATION_ACTION_SCHEMA,
)
@automation.register_action(
    "pas_co2.abc_enable", PasCo2ABCEnableAction, CALIBRATION_ACTION_SCHEMA
)
@automation.register_action(
    "pas_co2.abc_disable", PasCo2ABCDisableAction, CALIBRATION_ACTION_SCHEMA
)
async def pas_co2_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)
