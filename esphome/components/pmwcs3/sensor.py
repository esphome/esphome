import esphome.codegen as cg
from esphome import automation
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    CONF_ADDRESS,
    CONF_TEMPERATURE,
    CONF_EC,
    STATE_CLASS_MEASUREMENT,
    ICON_THERMOMETER,
)

CODEOWNERS = ["@SeByDocKy"]
DEPENDENCIES = ["i2c"]

CONF_E25 = "e25"
CONF_VWC = "vwc"

ICON_EPSILON = "mdi:epsilon"
ICON_SIGMA = "mdi:sigma-lower"
ICON_ALPHA = "mdi:alpha-h-circle-outline"

pmwcs3_ns = cg.esphome_ns.namespace("pmwcs3")
PMWCS3Component = pmwcs3_ns.class_(
    "PMWCS3Component", cg.PollingComponent, i2c.I2CDevice
)

# Actions
PMWCS3AirCalibrationAction = pmwcs3_ns.class_(
    "PMWCS3AirCalibrationAction", automation.Action
)
PMWCS3WaterCalibrationAction = pmwcs3_ns.class_(
    "PMWCS3WaterCalibrationAction", automation.Action
)
PMWCS3NewI2cAddressAction = pmwcs3_ns.class_(
    "PMWCS3NewI2cAddressAction", automation.Action
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PMWCS3Component),
            cv.Optional(CONF_E25): sensor.sensor_schema(
                icon=ICON_EPSILON,
                accuracy_decimals=3,
                unit_of_measurement="dS/m",
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_EC): sensor.sensor_schema(
                icon=ICON_SIGMA,
                accuracy_decimals=2,
                unit_of_measurement="mS/m",
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                icon=ICON_THERMOMETER,
                accuracy_decimals=3,
                unit_of_measurement="°C",
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_VWC): sensor.sensor_schema(
                icon=ICON_ALPHA,
                accuracy_decimals=3,
                unit_of_measurement="cm3cm−3",
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x63))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_E25 in config:
        sens = await sensor.new_sensor(config[CONF_E25])
        cg.add(var.set_e25_sensor(sens))

    if CONF_EC in config:
        sens = await sensor.new_sensor(config[CONF_EC])
        cg.add(var.set_ec_sensor(sens))

    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))

    if CONF_VWC in config:
        sens = await sensor.new_sensor(config[CONF_VWC])
        cg.add(var.set_vwc_sensor(sens))


# Actions
PMWCS3_CALIBRATION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(PMWCS3Component),
    }
)


@automation.register_action(
    "pmwcs3.air_calibration",
    PMWCS3AirCalibrationAction,
    PMWCS3_CALIBRATION_SCHEMA,
)
@automation.register_action(
    "pmwcs3.water_calibration",
    PMWCS3WaterCalibrationAction,
    PMWCS3_CALIBRATION_SCHEMA,
)
async def pmwcs3_calibration_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    return var


PMWCS3_NEW_I2C_ADDRESS_SCHEMA = cv.maybe_simple_value(
    {
        cv.GenerateID(): cv.use_id(PMWCS3Component),
        cv.Required(CONF_ADDRESS): cv.templatable(cv.i2c_address),
    },
    key=CONF_ADDRESS,
)


@automation.register_action(
    "pmwcs3.new_i2c_address",
    PMWCS3NewI2cAddressAction,
    PMWCS3_NEW_I2C_ADDRESS_SCHEMA,
)
async def pmwcs3newi2caddress_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    address = await cg.templatable(config[CONF_ADDRESS], args, int)
    cg.add(var.set_new_address(address))
    return var
