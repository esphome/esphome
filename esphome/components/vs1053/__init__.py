from esphome import pins
import esphome.codegen as cg
from esphome.components import spi
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_RESET_PIN

CODEOWNERS = ["@mill1000"]
DEPENDENCIES = ["spi"]

MULTI_CONF = True
CONF_VS1053_ID = "vs1053_id"
CONF_SDI_SPI = "sdi_spi"
CONF_SCI_SPI = "sci_spi"
CONF_DREQ_PIN = "dreq_pin"


vs1053_ns = cg.esphome_ns.namespace("vs1053")
VS1053Component = vs1053_ns.class_("VS1053Component", cg.Component)

VS1053_SDI_SPI = vs1053_ns.class_("VS1053SdiSpiDevice", spi.SPIDevice)
VS1053_SCI_SPI = vs1053_ns.class_("VS1053SciSpiDevice", spi.SPIDevice)


def _spi_schema(cls) -> cv.Schema:
    return cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(cls),
        }
    ).extend(spi.spi_device_schema(cs_pin_required=True))


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(VS1053Component),
        cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_DREQ_PIN): pins.gpio_input_pin_schema,
        cv.Required(CONF_SCI_SPI): _spi_schema(VS1053_SCI_SPI),
        cv.Required(CONF_SDI_SPI): _spi_schema(VS1053_SDI_SPI),
    }
).extend(cv.COMPONENT_SCHEMA)


async def spi_to_code(config):
    # Construct and register SPI devices
    var = cg.new_Pvariable(config[CONF_ID])
    await spi.register_spi_device(var, config)
    return var


async def to_code(config) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Set reset pin if present
    if reset_pin := config.get(CONF_RESET_PIN):
        cg.add(var.set_reset_pin(await cg.gpio_pin_expression(reset_pin)))

    # Set data request pin
    dreq_pin = await cg.gpio_pin_expression(config[CONF_DREQ_PIN])
    cg.add(var.set_dreq_pin(dreq_pin))

    # Register each SPI device
    sci_spi = await spi_to_code(config[CONF_SCI_SPI])
    cg.add(var.set_sci_device(sci_spi))

    sdi_spi = await spi_to_code(config[CONF_SDI_SPI])
    cg.add(var.set_sdi_device(sdi_spi))
