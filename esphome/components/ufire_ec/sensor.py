import esphome.codegen as cg
from esphome import automation
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    CONF_EC,
    CONF_TEMPERATURE,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_TEMPERATURE,
    ICON_EMPTY,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_MILLISIEMENS_PER_CENTIMETER,
)

DEPENDENCIES = ["i2c"]

CONF_SOLUTION = "solution"
CONF_TEMPERATURE_SENSOR = "temperature_sensor"
CONF_TEMPERATURE_COMPENSATION = "temperature_compensation"
CONF_TEMPERATURE_COEFFICIENT = "temperature_coefficient"

ufire_ec_ns = cg.esphome_ns.namespace("ufire_ec")
UFireECComponent = ufire_ec_ns.class_(
    "UFireECComponent", cg.PollingComponent, i2c.I2CDevice
)

# Actions
UFireECCalibrateProbeAction = ufire_ec_ns.class_(
    "UFireECCalibrateProbeAction", automation.Action
)
UFireECResetAction = ufire_ec_ns.class_("UFireECResetAction", automation.Action)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(UFireECComponent),
            cv.Exclusive(CONF_TEMPERATURE, "temperature"): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=1,
            ),
            cv.Optional(CONF_EC): sensor.sensor_schema(
                unit_of_measurement=UNIT_MILLISIEMENS_PER_CENTIMETER,
                icon=ICON_EMPTY,
                device_class=DEVICE_CLASS_EMPTY,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=1,
            ),
            cv.Exclusive(CONF_TEMPERATURE_SENSOR, "temperature"): cv.use_id(
                sensor.Sensor
            ),
            cv.Optional(CONF_TEMPERATURE_COMPENSATION, default=21.0): cv.temperature,
            cv.Optional(CONF_TEMPERATURE_COEFFICIENT, default=0.019): cv.float_range(
                min=0.01, max=0.04
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x3C))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_temperature_compensation(config[CONF_TEMPERATURE_COMPENSATION]))
    cg.add(var.set_temperature_coefficient(config[CONF_TEMPERATURE_COEFFICIENT]))

    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))

    if CONF_EC in config:
        sens = await sensor.new_sensor(config[CONF_EC])
        cg.add(var.set_ec_sensor(sens))

    if CONF_TEMPERATURE_SENSOR in config:
        sens = await cg.get_variable(config[CONF_TEMPERATURE_SENSOR])
        cg.add(var.set_temperature_sensor_external(sens))

    await i2c.register_i2c_device(var, config)


UFIRE_EC_CALIBRATE_PROBE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(UFireECComponent),
        cv.Required(CONF_SOLUTION): cv.templatable(float),
        cv.Required(CONF_TEMPERATURE): cv.templatable(cv.temperature),
    }
)


@automation.register_action(
    "ufire_ec.calibrate_probe",
    UFireECCalibrateProbeAction,
    UFIRE_EC_CALIBRATE_PROBE_SCHEMA,
)
async def ufire_ec_calibrate_probe_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    solution_ = await cg.templatable(config[CONF_SOLUTION], args, float)
    temperature_ = await cg.templatable(config[CONF_TEMPERATURE], args, float)
    cg.add(var.set_solution(solution_))
    cg.add(var.set_temperature(temperature_))
    return var


UFIRE_EC_RESET_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(UFireECComponent),
    }
)


@automation.register_action(
    "ufire_ec.reset",
    UFireECResetAction,
    UFIRE_EC_RESET_SCHEMA,
)
async def ufire_ec_reset_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    return var
