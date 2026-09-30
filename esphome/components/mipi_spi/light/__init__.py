import esphome.codegen as cg
from esphome.components import light
import esphome.config_validation as cv
from esphome.const import (
    CONF_BRIGHTNESS,
    CONF_DISPLAY_ID,
    CONF_GAMMA_CORRECT,
    CONF_MAX_BRIGHTNESS,
    CONF_MIN_BRIGHTNESS,
    CONF_MODEL,
    CONF_OUTPUT_ID,
)
import esphome.final_validate as fv
from esphome.types import ConfigType

from ..display import MipiSpi, mipi_spi_ns

MipiSpiLight = mipi_spi_ns.class_("MipiSpiLight", light.LightOutput)


def _validate_brightness_range(config: ConfigType) -> ConfigType:
    if config[CONF_MIN_BRIGHTNESS] >= config[CONF_MAX_BRIGHTNESS]:
        raise cv.Invalid(
            f"'{CONF_MIN_BRIGHTNESS}' must be less than '{CONF_MAX_BRIGHTNESS}'"
        )
    return config


CONFIG_SCHEMA = cv.All(
    light.BRIGHTNESS_ONLY_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(MipiSpiLight),
            cv.GenerateID(CONF_DISPLAY_ID): cv.use_id(MipiSpi),
            cv.Optional(CONF_GAMMA_CORRECT, default=1.0): cv.positive_float,
            cv.Optional(CONF_MIN_BRIGHTNESS, default=0): cv.int_range(0, 255),
            cv.Optional(CONF_MAX_BRIGHTNESS, default=255): cv.int_range(0, 255),
        }
    ),
    _validate_brightness_range,
)


def _final_validate(config: ConfigType) -> None:
    full_config = fv.full_config.get()
    display_path = full_config.get_path_for_id(config[CONF_DISPLAY_ID])[:-1]
    display_config = full_config.get_config_for_path(display_path)
    if CONF_BRIGHTNESS not in display_config:
        model = display_config[CONF_MODEL]
        if model == "CUSTOM":
            raise cv.Invalid(
                f"The '{CONF_BRIGHTNESS}' option must be set in the display config"
            )
        raise cv.Invalid(
            f"Display model {model} does not support setting brightness with an SPI command"
        )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(
        config[CONF_OUTPUT_ID],
        config[CONF_MIN_BRIGHTNESS],
        config[CONF_MAX_BRIGHTNESS],
    )
    await light.register_light(var, config)
    await cg.register_parented(var, config[CONF_DISPLAY_ID])
