"""DSP-408 text (read+write) sub-platform.

Kinds:
  * PRESET_NAME    -- 15-byte ASCII preset name (cmd=0x00 / cat=0x09)
  * CHANNEL_NAME   -- per-channel 8-byte ASCII name (cmd=0x2400+ch / cat=0x04),
                      pulled from blob[288..295] on warmup, written via
                      cmd=0x2400+ch.
"""

import esphome.codegen as cg
from esphome.components import text
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL

from .. import CONF_DSP408_ID, DSP408, dsp408_ns

DEPENDENCIES = ["dsp408"]

CONF_KIND = "kind"
KIND_PRESET_NAME = "PRESET_NAME"
KIND_CHANNEL_NAME = "CHANNEL_NAME"

KINDS = {
    KIND_PRESET_NAME: KIND_PRESET_NAME,
    KIND_CHANNEL_NAME: KIND_CHANNEL_NAME,
}

DSP408Text = dsp408_ns.class_("DSP408Text", text.Text, cg.Component)


def _validate(config):
    kind = config[CONF_KIND]
    if kind == KIND_CHANNEL_NAME and CONF_CHANNEL not in config:
        raise cv.Invalid("channel: 0..7 is required for kind: CHANNEL_NAME")
    if kind == KIND_PRESET_NAME and CONF_CHANNEL in config:
        raise cv.Invalid("channel: must NOT be set for kind: PRESET_NAME")
    return config


CONFIG_SCHEMA = cv.All(
    text.text_schema(DSP408Text).extend(
        {
            cv.GenerateID(CONF_DSP408_ID): cv.use_id(DSP408),
            cv.Required(CONF_KIND): cv.enum(KINDS, upper=True),
            cv.Optional(CONF_CHANNEL): cv.int_range(min=0, max=7),
        }
    ),
    _validate,
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_DSP408_ID])
    kind = config[CONF_KIND]
    if kind == KIND_PRESET_NAME:
        var = await text.new_text(config, max_length=15)
        cg.add(var.set_parent(parent))
        cg.add(var.set_preset_name())
        cg.add(parent.set_preset_name_text(var))
    else:  # CHANNEL_NAME
        ch = config[CONF_CHANNEL]
        var = await text.new_text(config, max_length=8)
        cg.add(var.set_parent(parent))
        cg.add(var.set_channel_name(ch))
        cg.add(parent.set_channel_name_text(ch, var))
