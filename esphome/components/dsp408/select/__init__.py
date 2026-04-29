"""DSP-408 select sub-platform.

Kinds:
  * CHANNEL_HPF_FILTER  -- HPF filter type per channel (Butterworth /
                           Bessel / Linkwitz-Riley)
  * CHANNEL_HPF_SLOPE   -- HPF slope per channel (6/12/18/24/30/36/42/48
                           dB/oct, or Off)
  * CHANNEL_LPF_FILTER  -- same for LPF
  * CHANNEL_LPF_SLOPE   -- same for LPF
"""

import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL

from .. import CONF_DSP408_ID, CONF_KIND, DSP408, dsp408_ns

DEPENDENCIES = ["dsp408"]

KIND_CHANNEL_HPF_FILTER = "CHANNEL_HPF_FILTER"
KIND_CHANNEL_HPF_SLOPE = "CHANNEL_HPF_SLOPE"
KIND_CHANNEL_LPF_FILTER = "CHANNEL_LPF_FILTER"
KIND_CHANNEL_LPF_SLOPE = "CHANNEL_LPF_SLOPE"

KINDS = {
    KIND_CHANNEL_HPF_FILTER: KIND_CHANNEL_HPF_FILTER,
    KIND_CHANNEL_HPF_SLOPE: KIND_CHANNEL_HPF_SLOPE,
    KIND_CHANNEL_LPF_FILTER: KIND_CHANNEL_LPF_FILTER,
    KIND_CHANNEL_LPF_SLOPE: KIND_CHANNEL_LPF_SLOPE,
}

# Per dsp408-py FILTER_TYPE_NAMES — note 2 and 3 are duplicates. The
# Windows GUI surfaces 3 distinct options; we follow that for HA UX.
FILTER_OPTIONS = ["Butterworth", "Bessel", "Linkwitz-Riley"]
SLOPE_OPTIONS = [
    "6 dB/oct",
    "12 dB/oct",
    "18 dB/oct",
    "24 dB/oct",
    "30 dB/oct",
    "36 dB/oct",
    "42 dB/oct",
    "48 dB/oct",
    "Off",
]

DSP408Select = dsp408_ns.class_("DSP408Select", select.Select, cg.Component)


def _validate(config):
    if CONF_CHANNEL not in config:
        raise cv.Invalid("channel: 0..7 is required")
    return config


CONFIG_SCHEMA = cv.All(
    select.select_schema(DSP408Select).extend(
        {
            cv.GenerateID(CONF_DSP408_ID): cv.use_id(DSP408),
            cv.Required(CONF_KIND): cv.enum(KINDS, upper=True),
            cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=7),
        }
    ),
    _validate,
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_DSP408_ID])
    kind = config[CONF_KIND]
    ch = config[CONF_CHANNEL]
    if kind in (KIND_CHANNEL_HPF_FILTER, KIND_CHANNEL_LPF_FILTER):
        var = await select.new_select(config, options=FILTER_OPTIONS)
    else:
        var = await select.new_select(config, options=SLOPE_OPTIONS)
    cg.add(var.set_parent(parent))
    if kind == KIND_CHANNEL_HPF_FILTER:
        cg.add(var.set_channel_hpf_filter(ch))
        cg.add(parent.set_channel_hpf_filter_select(ch, var))
    elif kind == KIND_CHANNEL_HPF_SLOPE:
        cg.add(var.set_channel_hpf_slope(ch))
        cg.add(parent.set_channel_hpf_slope_select(ch, var))
    elif kind == KIND_CHANNEL_LPF_FILTER:
        cg.add(var.set_channel_lpf_filter(ch))
        cg.add(parent.set_channel_lpf_filter_select(ch, var))
    elif kind == KIND_CHANNEL_LPF_SLOPE:
        cg.add(var.set_channel_lpf_slope(ch))
        cg.add(parent.set_channel_lpf_slope_select(ch, var))
