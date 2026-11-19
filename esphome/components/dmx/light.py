import esphome.codegen as cg
from esphome.components import light
from esphome.components.light import LightType
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TYPE
import esphome.final_validate as fv

from . import CONF_DMX_ID, DMXComponent, dmx_ns

DEPENDENCIES = ["dmx"]

CONF_FIRST_CHANNEL = "first_channel"

TYPE_DIMMER = "dimmer"
TYPE_RGB = "rgb"
TYPE_RGBW = "rgbw"

DMXLightOutput = dmx_ns.class_("DMXLightOutput", light.LightOutput)
DMXLightType = dmx_ns.enum("DMXLightType", is_class=True)

TYPE_TO_ENUM = {
    TYPE_DIMMER: DMXLightType.DIMMER,
    TYPE_RGB: DMXLightType.RGB,
    TYPE_RGBW: DMXLightType.RGBW,
}

DMX_OUTPUT_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_DMX_ID): cv.use_id(DMXComponent),
        cv.Required(CONF_FIRST_CHANNEL): cv.int_range(min=1, max=512),
    }
)


def _light_schema(light_type):
    return light.light_schema(
        DMXLightOutput,
        type_=light_type,
    ).extend(DMX_OUTPUT_SCHEMA)


CONFIG_SCHEMA = cv.typed_schema(
    {
        TYPE_DIMMER: _light_schema(LightType.BRIGHTNESS_ONLY),
        TYPE_RGB: _light_schema(LightType.RGB),
        TYPE_RGBW: _light_schema(LightType.RGB),
    },
    key=CONF_TYPE,
    default_type=TYPE_DIMMER,
)


def _validate_channel_range(config):
    count = {
        TYPE_DIMMER: 1,
        TYPE_RGB: 3,
        TYPE_RGBW: 4,
    }[config[CONF_TYPE]]

    if config[CONF_FIRST_CHANNEL] + count - 1 > 512:
        raise cv.Invalid(
            f"{config[CONF_TYPE]} starting at channel "
            f"{config[CONF_FIRST_CHANNEL]} would exceed DMX channel 512"
        )


def _final_validate(config) -> None:
    _validate_channel_range(config)

    full = fv.full_config.get()
    hubs = full.get("dmx", [])
    if not isinstance(hubs, list):
        hubs = [hubs] if hubs else []

    target = config[CONF_DMX_ID].id

    for hub in hubs:
        if hub[CONF_ID].id == target:
            if hub.get("mode", "rx") == "rx":
                raise cv.Invalid(
                    "dmx light requires hub mode 'tx' or 'bidirectional', not 'rx'"
                )
            break


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var = await light.new_light(config)

    parent = await cg.get_variable(config[CONF_DMX_ID])

    cg.add(var.set_parent(parent))
    cg.add(var.set_first_channel(config[CONF_FIRST_CHANNEL]))
    cg.add(var.set_light_type(TYPE_TO_ENUM[config[CONF_TYPE]]))
    cg.add(parent.enable_tx())
