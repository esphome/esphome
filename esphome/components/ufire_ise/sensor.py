import esphome.codegen as cg
from esphome import automation
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    CONF_PH,
    CONF_TEMPERATURE,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_TEMPERATURE,
    ICON_EMPTY,
    UNIT_CELSIUS,
    UNIT_PH,
)

DEPENDENCIES = ["i2c"]
CODEOWNERS = ["@pvizeli"]

CONF_SOLUTION = "solution"
CONF_TEMPERATURE_SENSOR = "temperature_sensor"

ufire_ise_ns = cg.esphome_ns.namespace("ufire_ise")
UFireISEComponent = ufire_ise_ns.class_(
    "UFireISEComponent", cg.PollingComponent, i2c.I2CDevice
)

# Actions
UFireISECalibrateProbeLowAction = ufire_ise_ns.class_(
    "UFireISECalibrateProbeLowAction", automation.Action
)
UFireISECalibrateProbeHighAction = ufire_ise_ns.class_(
    "UFireISECalibrateProbeHighAction", automation.Action
)
UFireISEResetAction = ufire_ise_ns.class_("UFireISEResetAction", automation.Action)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(UFireISEComponent),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                UNIT_CELSIUS, ICON_EMPTY, 1, DEVICE_CLASS_TEMPERATURE
            ),
            cv.Optional(CONF_PH): sensor.sensor_schema(
                UNIT_PH, ICON_EMPTY, 1, DEVICE_CLASS_EMPTY
            ),
            cv.Optional(CONF_TEMPERATURE_SENSOR): cv.use_id(sensor.Sensor),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x3F))
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)

    if CONF_TEMPERATURE in config:
        sens = yield sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))

    if CONF_PH in config:
        sens = yield sensor.new_sensor(config[CONF_PH])
        cg.add(var.set_ph_sensor(sens))

    if CONF_TEMPERATURE_SENSOR in config:
        sens = yield cg.get_variable(config[CONF_TEMPERATURE_SENSOR])
        cg.add(var.set_temperature_sensor_external(sens))

    yield i2c.register_i2c_device(var, config)


UFIRE_ISE_CALIBRATE_PROBE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(UFireISEComponent),
        cv.Required(CONF_SOLUTION): cv.templatable(float),
    }
)


@automation.register_action(
    "ufire_ise.calibrate_probe_low",
    UFireISECalibrateProbeLowAction,
    UFIRE_ISE_CALIBRATE_PROBE_SCHEMA,
)
def ufire_ise_calibrate_probe_low_to_code(config, action_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = yield cg.templatable(config[CONF_SOLUTION], args, float)
    cg.add(var.set_solution(template_))
    yield var


@automation.register_action(
    "ufire_ise.calibrate_probe_high",
    UFireISECalibrateProbeHighAction,
    UFIRE_ISE_CALIBRATE_PROBE_SCHEMA,
)
def ufire_ise_calibrate_probe_high_to_code(config, action_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = yield cg.templatable(config[CONF_SOLUTION], args, float)
    cg.add(var.set_solution(template_))
    yield var


UFIRE_ISE_RESET_SCHEMA = cv.Schema({cv.GenerateID(): cv.use_id(UFireISEComponent)})


@automation.register_action(
    "ufire_ise.reset",
    UFireISEResetAction,
    UFIRE_ISE_RESET_SCHEMA,
)
def ufire_ise_reset_to_code(config, action_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    yield var
