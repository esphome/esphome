import esphome.codegen as cg
from esphome.components import light
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.const import CONF_ID, CONF_OUTPUT_ID, CONF_TYPE

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

CONFIG_SCHEMA = light.RGB_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(DMXLightOutput),
        cv.Required(CONF_DMX_ID): cv.use_id(DMXComponent),
        cv.Optional(CONF_TYPE, default=TYPE_DIMMER): cv.one_of(
            TYPE_DIMMER, TYPE_RGB, TYPE_RGBW, lower=True
        ),
        cv.Required(CONF_FIRST_CHANNEL): cv.int_range(min=1, max=512),
    }
)


def _validate_channel_range(config):
    count = {TYPE_DIMMER: 1, TYPE_RGB: 3, TYPE_RGBW: 4}[config[CONF_TYPE]]
    if config[CONF_FIRST_CHANNEL] + count - 1 > 512:
        raise cv.Invalid(
            f"{config[CONF_TYPE]} starting at channel {config[CONF_FIRST_CHANNEL]} "
            f"would exceed DMX channel 512"
        )
    return config


CONFIG_SCHEMA = cv.All(CONFIG_SCHEMA, _validate_channel_range)


def _final_validate(config):
    full = fv.full_config.get()
    hubs = full.get("dmx", [])
    if not isinstance(hubs, list):
        hubs = [hubs] if hubs else []
    target = config[CONF_DMX_ID].id
    for hub in hubs:
        if hub[CONF_ID].id == target:
            if hub.get("mode", "rx") == "rx":
                raise cv.Invalid("dmx light requires hub mode 'tx' or 'bidirectional', not 'rx'")
            break
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var = await light.new_light(config)
    parent = await cg.get_variable(config[CONF_DMX_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_first_channel(config[CONF_FIRST_CHANNEL]))
    cg.add(var.set_light_type(TYPE_TO_ENUM[config[CONF_TYPE]]))
    cg.add(parent.enable_tx())
