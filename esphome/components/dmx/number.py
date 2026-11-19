import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID, ICON_BRIGHTNESS_5, UNIT_EMPTY
import esphome.final_validate as fv

from . import CONF_DMX_ID, CONF_MODE, MODE_RX, DMXComponent, dmx_ns

DEPENDENCIES = ["dmx"]

DMXOutputNumber = dmx_ns.class_("DMXOutputNumber", number.Number, cg.Component)

CONFIG_SCHEMA = (
    number.number_schema(
        DMXOutputNumber,
        unit_of_measurement=UNIT_EMPTY,
        icon=ICON_BRIGHTNESS_5,
    )
    .extend(
        {
            cv.GenerateID(CONF_DMX_ID): cv.use_id(DMXComponent),
            cv.Required(CONF_CHANNEL): cv.int_range(min=1, max=512),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


def _final_validate(config):
    full = fv.full_config.get()
    hubs = full.get("dmx", [])
    if not isinstance(hubs, list):
        hubs = [hubs] if hubs else []
    target = config[CONF_DMX_ID].id
    for hub in hubs:
        if hub[CONF_ID].id == target:
            if hub.get(CONF_MODE, "rx") == MODE_RX:
                raise cv.Invalid(
                    "dmx number requires hub mode 'tx' or 'bidirectional', not 'rx'"
                )
            break
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await number.register_number(
        var,
        config,
        min_value=0,
        max_value=255,
        step=1,
    )

    parent = await cg.get_variable(config[CONF_DMX_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(parent.enable_tx())
