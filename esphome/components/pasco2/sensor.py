import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome import automation
from esphome.automation import maybe_simple_id
from esphome import pins

from esphome.const import (
    CONF_ID,
    CONF_CO2,
    CONF_VALUE,
    DEVICE_CLASS_CARBON_DIOXIDE,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_MILLION,
    ICON_MOLECULE_CO2,
    CONF_ENABLE_PIN,
)

CODEOWNERS = ["@circuitvalley"]
DEPENDENCIES = ["i2c"]

pasco2_ns = cg.esphome_ns.namespace("pasco2")
PASCO2Component = pasco2_ns.class_(
    "PASCO2Component", cg.PollingComponent, i2c.I2CDevice
)
MeasurementMode = pasco2_ns.enum("MEASUREMENT_MODE")
MEASUREMENT_MODE_OPTIONS = {
    "periodic": MeasurementMode.PERIODIC,
    "single_shot": MeasurementMode.SINGLE_SHOT,
}

# Actions
PerformForcedCalibrationAction = pasco2_ns.class_(
    "PerformForcedCalibrationAction", automation.Action
)
FactoryResetAction = pasco2_ns.class_("FactoryResetAction", automation.Action)

CONF_AMBIENT_PRESSURE_COMPENSATION = "ambient_pressure_compensation"
CONF_AMBIENT_PRESSURE_COMPENSATION_SOURCE = "ambient_pressure_compensation_source"
CONF_AUTOMATIC_SELF_CALIBRATION = "automatic_self_calibration"
CONF_MEASUREMENT_MODE = "measurement_mode"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PASCO2Component),
            cv.Required(CONF_CO2): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_MILLION,
                icon=ICON_MOLECULE_CO2,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_CARBON_DIOXIDE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_AUTOMATIC_SELF_CALIBRATION, default=True): cv.boolean,
            cv.Optional(CONF_AMBIENT_PRESSURE_COMPENSATION): cv.pressure,
            cv.Optional(CONF_ENABLE_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_AMBIENT_PRESSURE_COMPENSATION_SOURCE): cv.use_id(
                sensor.Sensor
            ),
            cv.Optional(CONF_MEASUREMENT_MODE, default="periodic"): cv.enum(
                MEASUREMENT_MODE_OPTIONS, lower=True
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x28))
)

SENSOR_MAP = {
    CONF_CO2: "set_co2_sensor",
}

SETTING_MAP = {
    CONF_AUTOMATIC_SELF_CALIBRATION: "set_automatic_self_calibration",
    CONF_AMBIENT_PRESSURE_COMPENSATION: "set_ambient_pressure_compensation",
}


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    for key, funcName in SETTING_MAP.items():
        if key in config:
            cg.add(getattr(var, funcName)(config[key]))

    for key, funcName in SENSOR_MAP.items():
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(var, funcName)(sens))

    if CONF_AMBIENT_PRESSURE_COMPENSATION_SOURCE in config:
        sens = await cg.get_variable(config[CONF_AMBIENT_PRESSURE_COMPENSATION_SOURCE])
        cg.add(var.set_ambient_pressure_source(sens))

    if CONF_ENABLE_PIN in config:
        enable = await cg.gpio_pin_expression(config[CONF_ENABLE_PIN])
        cg.add(var.set_enable_pin(enable))

    cg.add(var.set_measurement_mode(config[CONF_MEASUREMENT_MODE]))


PASCO2_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(PASCO2Component),
        cv.Required(CONF_VALUE): cv.templatable(cv.positive_int),
    }
)


@automation.register_action(
    "pasco2.perform_forced_calibration",
    PerformForcedCalibrationAction,
    PASCO2_ACTION_SCHEMA,
)
async def pasco2_frc_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    template_ = await cg.templatable(config[CONF_VALUE], args, cg.uint16)
    cg.add(var.set_value(template_))
    return var


PASCO2_RESET_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(PASCO2Component),
    }
)
