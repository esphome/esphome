import esphome.codegen as cg
from esphome import automation
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    ICON_RADIATOR,
    ICON_RESTART,
    DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS_PARTS,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_BILLION,
    CONF_ADDRESS,
    CONF_TVOC,
    CONF_VERSION,
)

CONF_RESISTANCE = "resistance"
CONF_MODE = "mode"
CONF_VALUE = "value"

DEPENDENCIES = ["i2c"]

ags10_ns = cg.esphome_ns.namespace("ags10")
AGS10Component = ags10_ns.class_("AGS10Component", cg.PollingComponent, i2c.I2CDevice)

# Actions
AGS10NewI2cAddressAction = ags10_ns.class_(
    "AGS10NewI2cAddressAction", automation.Action
)
AGS10SetZeroPointAction = ags10_ns.class_("AGS10SetZeroPointAction", automation.Action)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(AGS10Component),
            cv.Required(CONF_TVOC): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_BILLION,
                icon=ICON_RADIATOR,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS_PARTS,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_VERSION): sensor.sensor_schema(
                icon=ICON_RESTART,
            ),
            cv.Optional(CONF_RESISTANCE): sensor.sensor_schema(
                icon=ICON_RESTART,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x1A))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    sens = await sensor.new_sensor(config[CONF_TVOC])
    cg.add(var.set_tvoc(sens))

    if version_config := config.get(CONF_VERSION):
        sens = await sensor.new_sensor(version_config)
        cg.add(var.set_version(sens))

    if resistance_config := config.get(CONF_RESISTANCE):
        sens = await sensor.new_sensor(resistance_config)
        cg.add(var.set_resistance(sens))


AGS10_NEW_I2C_ADDRESS_SCHEMA = cv.maybe_simple_value(
    {
        cv.GenerateID(): cv.use_id(AGS10Component),
        cv.Required(CONF_ADDRESS): cv.templatable(cv.i2c_address),
    },
    key=CONF_ADDRESS,
)


@automation.register_action(
    "ags10.new_i2c_address",
    AGS10NewI2cAddressAction,
    AGS10_NEW_I2C_ADDRESS_SCHEMA,
)
async def ags10newi2caddress_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    address = await cg.templatable(config[CONF_ADDRESS], args, int)
    cg.add(var.set_new_address(address))
    return var


AGS10SetZeroPointActionMode = ags10_ns.enum("AGS10SetZeroPointActionMode")
AGS10_SET_ZERO_POINT_ACTION_MODE = {
    "FACTORY_DEFAULT": AGS10SetZeroPointActionMode.FACTORY_DEFAULT,
    "CURRENT_VALUE": AGS10SetZeroPointActionMode.CURRENT_VALUE,
    "CUSTOM_VALUE": AGS10SetZeroPointActionMode.CUSTOM_VALUE,
}

AGS10_SET_ZERO_POINT_SCHEMA = cv.All(
    {
        cv.GenerateID(): cv.use_id(AGS10Component),
        cv.Required(CONF_MODE): cv.enum(
            AGS10_SET_ZERO_POINT_ACTION_MODE, upper=True
        ),
        cv.Optional(CONF_VALUE, default=0xFFFF): cv.templatable(cv.uint16_t),
    },
)


@automation.register_action(
    "ags10.set_zero_point",
    AGS10SetZeroPointAction,
    AGS10_SET_ZERO_POINT_SCHEMA,
)
async def ags10setzeropoint_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    var.set_mode(await cg.templatable(config.get(CONF_MODE), args, enumerate))
    if value := config.get(CONF_VALUE):
        cg.add(var.set_value(await cg.templatable(value, args, int)))
    return var
