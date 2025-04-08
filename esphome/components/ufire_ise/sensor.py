from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_PH,
    CONF_TEMPERATURE,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_TEMPERATURE,
    ICON_EMPTY,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PH,
)

DEPENDENCIES = ["i2c"]

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
            cv.Exclusive(CONF_TEMPERATURE, "temperature"): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=1,
            ),
            cv.Optional(CONF_PH): sensor.sensor_schema(
                unit_of_measurement=UNIT_PH,
                icon=ICON_EMPTY,
                device_class=DEVICE_CLASS_EMPTY,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=1,
            ),
            cv.Exclusive(CONF_TEMPERATURE_SENSOR, "temperature"): cv.use_id(
                sensor.Sensor
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x3F))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))

    if CONF_PH in config:
        sens = await sensor.new_sensor(config[CONF_PH])
        cg.add(var.set_ph_sensor(sens))

    if CONF_TEMPERATURE_SENSOR in config:
        sens = await cg.get_variable(config[CONF_TEMPERATURE_SENSOR])
        cg.add(var.set_temperature_sensor_external(sens))

    await i2c.register_i2c_device(var, config)


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
async def ufire_ise_calibrate_probe_low_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_SOLUTION], args, float)
    cg.add(var.set_solution(template_))
    return var


@automation.register_action(
    "ufire_ise.calibrate_probe_high",
    UFireISECalibrateProbeHighAction,
    UFIRE_ISE_CALIBRATE_PROBE_SCHEMA,
)
async def ufire_ise_calibrate_probe_high_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_SOLUTION], args, float)
    cg.add(var.set_solution(template_))
    return var


UFIRE_ISE_RESET_SCHEMA = cv.Schema({cv.GenerateID(): cv.use_id(UFireISEComponent)})


@automation.register_action(
    "ufire_ise.reset",
    UFireISEResetAction,
    UFIRE_ISE_RESET_SCHEMA,
)
async def ufire_ise_reset_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    return var
