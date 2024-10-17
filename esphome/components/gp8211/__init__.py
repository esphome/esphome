import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.components import i2c
from esphome.const import CONF_ID, CONF_VOLTAGE

CODEOWNERS = ["@haudamekki"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

gp8211_ns = cg.esphome_ns.namespace("gp8211")
GP8211 = gp8211_ns.class_("GP8211", cg.Component, i2c.I2CDevice)

GP8211Voltage = gp8211_ns.enum("GP8211Voltage")

CONF_GP8211_ID = "gp8211_id"

VOLTAGES = {
    "5V": GP8211Voltage.GP8211_VOLTAGE_5V,
    "10V": GP8211Voltage.GP8211_VOLTAGE_10V,
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(GP8211),
            cv.Required(CONF_VOLTAGE): cv.enum(VOLTAGES, upper=True),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x58))  # GP8211 I2C-Adresse
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_voltage(config[CONF_VOLTAGE]))
