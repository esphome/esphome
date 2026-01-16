import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MODEL, CONF_VOLTAGE

CODEOWNERS = ["@jesserockz", "@sebydocky"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

gp8403_ns = cg.esphome_ns.namespace("gp8403")
GP8403Component = gp8403_ns.class_("GP8403Component", cg.Component, i2c.I2CDevice)

GP8403Voltage = gp8403_ns.enum("GP8403Voltage")
GP8403Model = gp8403_ns.enum("GP8403Model")

CONF_GP8403_ID = "gp8403_id"

MODELS = {
    "GP8403": GP8403Model.GP8403,
    "GP8413": GP8403Model.GP8413,
}

VOLTAGES = {
    "5V": GP8403Voltage.GP8403_VOLTAGE_5V,
    "10V": GP8403Voltage.GP8403_VOLTAGE_10V,
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(GP8403Component),
            cv.Optional(CONF_MODEL, default="GP8403"): cv.enum(MODELS, upper=True),
            cv.Required(CONF_VOLTAGE): cv.enum(VOLTAGES, upper=True),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x58))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_model(config[CONF_MODEL]))
    cg.add(var.set_voltage(config[CONF_VOLTAGE]))
