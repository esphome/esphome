"""DSP-408 number sub-platform.

Kinds:
  * MASTER_VOLUME      -- master volume in dB, range -60..+6 (1 dB step)
  * CHANNEL_VOLUME     -- per-channel output volume, -60..0 dB
  * CHANNEL_DELAY      -- per-channel delay in samples, 0..359
                          (1 sample = ~22.7us @ 44.1 kHz; firmware caps
                          at 359 ≈ 8.143 ms)
  * CHANNEL_HPF_FREQ   -- per-channel high-pass cutoff in Hz, 10..20000
  * CHANNEL_LPF_FREQ   -- per-channel low-pass cutoff in Hz, 100..22000
"""

import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL

from .. import CONF_DSP408_ID, CONF_KIND, DSP408, dsp408_ns

DEPENDENCIES = ["dsp408"]

KIND_MASTER_VOLUME = "MASTER_VOLUME"
KIND_CHANNEL_VOLUME = "CHANNEL_VOLUME"
KIND_CHANNEL_DELAY = "CHANNEL_DELAY"
KIND_CHANNEL_HPF_FREQ = "CHANNEL_HPF_FREQ"
KIND_CHANNEL_LPF_FREQ = "CHANNEL_LPF_FREQ"
KIND_CHANNEL_COMP_ATTACK = "CHANNEL_COMP_ATTACK"
KIND_CHANNEL_COMP_RELEASE = "CHANNEL_COMP_RELEASE"
KIND_CHANNEL_COMP_THRESHOLD = "CHANNEL_COMP_THRESHOLD"

KINDS = {
    KIND_MASTER_VOLUME: KIND_MASTER_VOLUME,
    KIND_CHANNEL_VOLUME: KIND_CHANNEL_VOLUME,
    KIND_CHANNEL_DELAY: KIND_CHANNEL_DELAY,
    KIND_CHANNEL_HPF_FREQ: KIND_CHANNEL_HPF_FREQ,
    KIND_CHANNEL_LPF_FREQ: KIND_CHANNEL_LPF_FREQ,
    KIND_CHANNEL_COMP_ATTACK: KIND_CHANNEL_COMP_ATTACK,
    KIND_CHANNEL_COMP_RELEASE: KIND_CHANNEL_COMP_RELEASE,
    KIND_CHANNEL_COMP_THRESHOLD: KIND_CHANNEL_COMP_THRESHOLD,
}

PER_CHANNEL_KINDS = {
    KIND_CHANNEL_VOLUME,
    KIND_CHANNEL_DELAY,
    KIND_CHANNEL_HPF_FREQ,
    KIND_CHANNEL_LPF_FREQ,
    KIND_CHANNEL_COMP_ATTACK,
    KIND_CHANNEL_COMP_RELEASE,
    KIND_CHANNEL_COMP_THRESHOLD,
}

DSP408Number = dsp408_ns.class_("DSP408Number", number.Number, cg.Component)
NumberKind = dsp408_ns.namespace("NumberKind")


def _validate(config):
    kind = config[CONF_KIND]
    if kind in PER_CHANNEL_KINDS and CONF_CHANNEL not in config:
        raise cv.Invalid(f"channel: 0..7 is required for kind: {kind}")
    if kind == KIND_MASTER_VOLUME and CONF_CHANNEL in config:
        raise cv.Invalid("channel: must NOT be set for kind: MASTER_VOLUME")
    return config


CONFIG_SCHEMA = cv.All(
    number.number_schema(DSP408Number).extend(
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
    if kind == KIND_MASTER_VOLUME:
        # Master volume: firmware encoding is integer dB (1 dB step in
        # the 8-byte payload). Step 1.0 keeps the slider sensible.
        var = await number.new_number(config, min_value=-60.0, max_value=6.0, step=1.0)
        cg.add(var.set_parent(parent))
        cg.add(var.set_master_volume())
        cg.add(parent.set_master_volume_number(var))
    elif kind == KIND_CHANNEL_VOLUME:
        # Per-channel volume: firmware encodes raw = (dB×10)+600 in u16,
        # giving 0.1 dB native resolution. We expose 0.5 dB step in HA
        # as a UX compromise — finer than 1 dB but coarse enough that
        # slider drag doesn't generate excessive bus traffic.
        ch = config[CONF_CHANNEL]
        var = await number.new_number(config, min_value=-60.0, max_value=0.0, step=0.5)
        cg.add(var.set_parent(parent))
        cg.add(var.set_channel_volume(ch))
        cg.add(parent.set_channel_volume_number(ch, var))
    elif kind == KIND_CHANNEL_DELAY:
        ch = config[CONF_CHANNEL]
        var = await number.new_number(config, min_value=0.0, max_value=359.0, step=1.0)
        cg.add(var.set_parent(parent))
        cg.add(var.set_channel_delay(ch))
        cg.add(parent.set_channel_delay_number(ch, var))
    elif kind == KIND_CHANNEL_HPF_FREQ:
        ch = config[CONF_CHANNEL]
        var = await number.new_number(
            config, min_value=10.0, max_value=20000.0, step=1.0
        )
        cg.add(var.set_parent(parent))
        cg.add(var.set_channel_hpf_freq(ch))
        cg.add(parent.set_channel_hpf_freq_number(ch, var))
    elif kind == KIND_CHANNEL_LPF_FREQ:
        ch = config[CONF_CHANNEL]
        var = await number.new_number(
            config, min_value=100.0, max_value=22000.0, step=1.0
        )
        cg.add(var.set_parent(parent))
        cg.add(var.set_channel_lpf_freq(ch))
        cg.add(parent.set_channel_lpf_freq_number(ch, var))
    elif kind == KIND_CHANNEL_COMP_ATTACK:
        ch = config[CONF_CHANNEL]
        var = await number.new_number(config, min_value=0.0, max_value=2000.0, step=1.0)
        cg.add(var.set_parent(parent))
        cg.add(var.set_channel_comp_attack(ch))
        cg.add(parent.set_channel_comp_attack_number(ch, var))
    elif kind == KIND_CHANNEL_COMP_RELEASE:
        ch = config[CONF_CHANNEL]
        var = await number.new_number(
            config, min_value=0.0, max_value=10000.0, step=1.0
        )
        cg.add(var.set_parent(parent))
        cg.add(var.set_channel_comp_release(ch))
        cg.add(parent.set_channel_comp_release_number(ch, var))
    elif kind == KIND_CHANNEL_COMP_THRESHOLD:
        ch = config[CONF_CHANNEL]
        var = await number.new_number(config, min_value=0.0, max_value=255.0, step=1.0)
        cg.add(var.set_parent(parent))
        cg.add(var.set_channel_comp_threshold(ch))
        cg.add(parent.set_channel_comp_threshold_number(ch, var))
