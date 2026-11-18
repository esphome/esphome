import esphome.codegen as cg

CODEOWNERS = ["@clydebarrow"]

DOMAIN = "mipi_spi"

register_enable_pin = cg.slot_list(
    "MIPI_SPI_ENABLE_PIN_COUNT", "register_enable_pin", DOMAIN
)

CONF_SPI_16 = "spi_16"
CONF_BUS_MODE = "bus_mode"
