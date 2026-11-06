import esphome.codegen as cg
from esphome.components import light, spi
import esphome.config_validation as cv
from esphome.const import CONF_NUM_LEDS, CONF_OUTPUT_ID
from esphome.core import CORE
from esphome.types import ConfigType

spi_led_strip_ns = cg.esphome_ns.namespace("spi_led_strip")
SpiLedStrip = spi_led_strip_ns.class_(
    "SpiLedStrip", light.AddressableLight, spi.SPIDevice
)

CONFIG_SCHEMA = light.ADDRESSABLE_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(SpiLedStrip),
        cv.Optional(CONF_NUM_LEDS, default=1): cv.positive_not_null_int,
    }
).extend(spi.spi_device_schema(False, "1MHz"))


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID], config[CONF_NUM_LEDS])
    await light.register_light(var, config)
    await spi.register_spi_device(var, config)
    await cg.register_component(var, config)
    # With PSRAM, setup() allocates the buffer there instead
    if "psram" not in CORE.loaded_integrations:
        buffer = cg.static_buffer(
            config[CONF_OUTPUT_ID],
            cg.uint32,
            str(spi_led_strip_ns.buffer_words(config[CONF_NUM_LEDS])),
        )
        cg.add(var.set_buffer(buffer))
