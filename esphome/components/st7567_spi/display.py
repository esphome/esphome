from esphome import pins
import esphome.codegen as cg
from esphome.components import spi, st7567_base
import esphome.config_validation as cv
from esphome.const import CONF_DC_PIN, CONF_ID, CONF_LAMBDA, CONF_PAGES

CODEOWNERS = ["@latonita"]

AUTO_LOAD = ["st7567_base"]
DEPENDENCIES = ["spi"]

st7567_spi = cg.esphome_ns.namespace("st7567_spi")
SPIST7567 = st7567_spi.class_("SPIST7567", st7567_base.ST7567, spi.SPIDevice)

CONFIG_SCHEMA = cv.All(
    st7567_base.ST7567_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(SPIST7567),
            cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema()),
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
)

FINAL_VALIDATE_SCHEMA = spi.final_validate_device_schema(
    "st7567_spi", require_miso=False, require_mosi=True
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await st7567_base.setup_st7567(var, config)
    await spi.register_spi_device(var, config)

    dc = await cg.gpio_pin_expression(config[CONF_DC_PIN])
    cg.add(var.set_dc_pin(dc))
