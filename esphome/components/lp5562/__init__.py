import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.types import ConfigType

CODEOWNERS = ["@DennisGaida"]
DEPENDENCIES = ["i2c"]
DOMAIN = "lp5562"
MULTI_CONF = True

CONF_LOGARITHMIC_DIMMING = "logarithmic_dimming"

lp5562_ns = cg.esphome_ns.namespace("lp5562")
LP5562Output = lp5562_ns.class_("LP5562Output", cg.Component, i2c.I2CDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LP5562Output),
            cv.Optional(CONF_LOGARITHMIC_DIMMING, default=False): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x30))
)


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)
    if config[CONF_LOGARITHMIC_DIMMING]:
        cg.add(var.set_logarithmic_dimming(True))
