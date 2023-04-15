import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID

CODEOWNERS = ["@Szewcson"]

DEPENDENCIES = ["i2c"]
MULTI_CONF = True

CONF_GDK101_ID = "gdk101_id"
CONF_RADIATION_DOSE_PER_1M = "radiation_dose_per_1m"
CONF_RADIATION_DOSE_PER_10M = "radiation_dose_per_10m"
CONF_VIBRATIONS = "vibrations"
CONF_MEAS_TIME = "meas_time"


gdk101_ns = cg.esphome_ns.namespace("gdk101")
GDK101Component = gdk101_ns.class_(
    "GDK101Component", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(GDK101Component),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x18))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
