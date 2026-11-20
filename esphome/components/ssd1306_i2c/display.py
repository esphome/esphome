import esphome.codegen as cg
from esphome.components import i2c, ssd1306_base
from esphome.components.ssd1306_base import _validate
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_MODEL, CONF_PAGES
from esphome.types import ConfigType

AUTO_LOAD = ["ssd1306_base"]
DEPENDENCIES = ["i2c"]

CONF_PARTIAL_UPDATES = "partial_updates"

PARTIAL_UPDATE_MODELS = frozenset(
    {
        "SSD1306_128X32",
        "SSD1306_128X64",
        "SSD1306_96X16",
        "SSD1306_64X48",
        "SSD1306_64X32",
        "SSD1306_72X40",
    }
)

ssd1306_i2c = cg.esphome_ns.namespace("ssd1306_i2c")
I2CSSD1306 = ssd1306_i2c.class_("I2CSSD1306", ssd1306_base.SSD1306, i2c.I2CDevice)


def _validate_partial_updates(config: ConfigType) -> ConfigType:
    if config[CONF_PARTIAL_UPDATES] and config[CONF_MODEL] not in PARTIAL_UPDATE_MODELS:
        raise cv.Invalid(
            "partial_updates is currently supported only for SSD1306 models"
        )
    return config


CONFIG_SCHEMA = cv.All(
    ssd1306_base.SSD1306_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(I2CSSD1306),
            cv.Optional(CONF_PARTIAL_UPDATES, default=False): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x3C)),
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
    _validate,
    _validate_partial_updates,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await ssd1306_base.setup_ssd1306(var, config)
    await i2c.register_i2c_device(var, config)
    if config[CONF_PARTIAL_UPDATES]:
        cg.add_define("USE_SSD1306_I2C_PARTIAL_UPDATES")
        cg.add(var.set_partial_updates())
