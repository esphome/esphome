import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light
from esphome.components import spi
from esphome.const import CONF_OUTPUT_ID, CONF_NUM_LEDS, CONF_DATA_RATE

spi_led_strip_ns = cg.esphome_ns.namespace("spi_led_strip")
SpiLedStrip = spi_led_strip_ns.class_(
    "SpiLedStrip", light.AddressableLight, spi.SPIDevice
)

CONFIG_SCHEMA = light.ADDRESSABLE_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(SpiLedStrip),
        cv.Optional(CONF_NUM_LEDS, default=1): cv.positive_not_null_int,
        cv.Optional(CONF_DATA_RATE, default="1MHz"): spi.SPI_DATA_RATE_SCHEMA,
    }
).extend(spi.spi_device_schema(False))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    cg.add(var.set_data_rate(spi.SPI_DATA_RATE_OPTIONS[config[CONF_DATA_RATE]]))
    cg.add(var.set_num_leds(config[CONF_NUM_LEDS]))
    await light.register_light(var, config)
    await spi.register_spi_device(var, config)
    await cg.register_component(var, config)
