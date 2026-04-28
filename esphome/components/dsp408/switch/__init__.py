"""DSP-408 switch sub-platform.

Kinds:
  * MASTER_MUTE      -- toggle master mute
  * CHANNEL_MUTE     -- per-channel mute (output side)
  * CHANNEL_POLAR    -- per-channel phase invert (output side, 180°)
  * INPUT_POLAR      -- per-input phase invert (cat=0x03 MISC plane)
"""

import esphome.codegen as cg
from esphome.components import switch as switch_
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL

from .. import CONF_DSP408_ID, DSP408, dsp408_ns

DEPENDENCIES = ["dsp408"]

CONF_KIND = "kind"
KIND_MASTER_MUTE = "MASTER_MUTE"
KIND_CHANNEL_MUTE = "CHANNEL_MUTE"
KIND_CHANNEL_POLAR = "CHANNEL_POLAR"
KIND_INPUT_POLAR = "INPUT_POLAR"

KINDS = {
    KIND_MASTER_MUTE: KIND_MASTER_MUTE,
    KIND_CHANNEL_MUTE: KIND_CHANNEL_MUTE,
    KIND_CHANNEL_POLAR: KIND_CHANNEL_POLAR,
    KIND_INPUT_POLAR: KIND_INPUT_POLAR,
}

PER_CHANNEL_KINDS = {KIND_CHANNEL_MUTE, KIND_CHANNEL_POLAR, KIND_INPUT_POLAR}

DSP408Switch = dsp408_ns.class_("DSP408Switch", switch_.Switch, cg.Component)


def _validate(config):
    kind = config[CONF_KIND]
    if kind in PER_CHANNEL_KINDS and CONF_CHANNEL not in config:
        raise cv.Invalid(f"channel: 0..7 is required for kind: {kind}")
    if kind == KIND_MASTER_MUTE and CONF_CHANNEL in config:
        raise cv.Invalid("channel: must NOT be set for kind: MASTER_MUTE")
    return config


CONFIG_SCHEMA = cv.All(
    switch_.switch_schema(DSP408Switch).extend(
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
    var = await switch_.new_switch(config)
    cg.add(var.set_parent(parent))
    kind = config[CONF_KIND]
    if kind == KIND_MASTER_MUTE:
        cg.add(var.set_master_mute())
        cg.add(parent.set_master_mute_switch(var))
    elif kind == KIND_CHANNEL_MUTE:
        ch = config[CONF_CHANNEL]
        cg.add(var.set_channel_mute(ch))
        cg.add(parent.set_channel_mute_switch(ch, var))
    elif kind == KIND_CHANNEL_POLAR:
        ch = config[CONF_CHANNEL]
        cg.add(var.set_channel_polar(ch))
        cg.add(parent.set_channel_polar_switch(ch, var))
    elif kind == KIND_INPUT_POLAR:
        ch = config[CONF_CHANNEL]
        cg.add(var.set_input_polar(ch))
        cg.add(parent.set_input_polar_switch(ch, var))
