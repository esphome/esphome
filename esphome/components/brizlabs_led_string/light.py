from esphome import pins
import esphome.codegen as cg
from esphome.components import light
from esphome.components.spi import (
    CONF_INTERFACE,
    CONF_INTERFACE_INDEX,
    get_spi_interface,
)
import esphome.config_validation as cv
from esphome.const import CONF_ETHERNET, CONF_OUTPUT_ID, CONF_PIN, CONF_SPI
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import DOMAIN

# BrizLabs two-wire LED string: color frames are pulse-coded onto the supply line via SPI MOSI.
# One addressable pixel per zone (1+ consecutive LEDs, repeating every num_zones * x LEDs).
CODEOWNERS = ["@TFenby"]
DEPENDENCIES = ["esp32"]

CONF_NUM_ZONES = "num_zones"

# The SPI host that setup() takes for itself, so no spi bus can use it.
SPI_HOST = "SPI2_HOST"

ns = cg.esphome_ns.namespace("brizlabs_led_string")
BrizLabsLEDString = ns.class_("BrizLabsLEDString", light.AddressableLight)

CONFIG_SCHEMA = light.ADDRESSABLE_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(BrizLabsLEDString),
        cv.Required(CONF_PIN): pins.internal_gpio_output_pin_number,
        cv.Optional(CONF_NUM_ZONES, default=6): cv.int_range(min=1, max=15),
        cv.Optional(light.CONF_POWER_ESTIMATION): cv.invalid(
            "power_estimation is not supported on brizlabs_led_string "
            "because each pixel represents a zone, not one LED"
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


def _final_validate_spi(config: ConfigType) -> ConfigType:
    full_config = fv.full_config.get()
    for spi_conf in full_config.get(CONF_SPI, []):
        index = spi_conf.get(CONF_INTERFACE_INDEX)
        if index is not None and get_spi_interface(index) == SPI_HOST:
            raise cv.Invalid(
                f"{DOMAIN} and an `spi` bus are both using {SPI_HOST}. "
                "Set `interface` on that bus to another hardware interface or to `software`."
            )
    # ethernet calls spi_bus_initialize directly if given clk/mosi/miso pins, so if someone some day wants to
    # use that and also this very specific style of light strip, we've gotta let them know
    interface = full_config.get(CONF_ETHERNET, {}).get(CONF_INTERFACE)
    if interface is not None and f"{interface.upper()}_HOST" == SPI_HOST:
        raise cv.Invalid(
            f"{DOMAIN} and `ethernet` are both using {SPI_HOST}. "
            "Set `interface` on `ethernet` to another hardware interface."
        )
    return config


FINAL_VALIDATE_SCHEMA = cv.All(
    fv.unique_platform_key(
        domain=light.DOMAIN,
        platform=DOMAIN,
        message=f"Only one {DOMAIN} is supported (it uses {SPI_HOST})",
    ),
    _final_validate_spi,
)


async def to_code(config: ConfigType) -> None:
    # Sizes the per-zone arrays. FINAL_VALIDATE_SCHEMA allows only one instance, so one define is enough.
    cg.add_define("BRIZLABS_LED_STRING_ZONE_COUNT", config[CONF_NUM_ZONES])
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID], config[CONF_PIN])
    await cg.register_component(var, config)
    await light.register_light(var, config)
