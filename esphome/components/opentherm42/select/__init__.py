import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import CONF_INITIAL_OPTION, ENTITY_CATEGORY_CONFIG

from .. import OpenTherm42Hub, opentherm42_ns
from ..const import (
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_SOLAR_STORAGE_STATUS_SOLAR_MODE,
    CONF_OPENTHERM42_ID,
)

OpenTherm42Select = opentherm42_ns.class_(
    "OpenTherm42Select", select.Select, cg.Component
)

# §5.3.1 Class 1, ID 101 HB bits 2,1,0: Master Solar Storage status: Solar mode -- master-authored,
# same "R -" idiom as ID 0/70's master status (see hub.h's RequestKind::SOLAR_STORAGE_STATUS
# comment): the wire message type is always READ-DATA, but the byte's content is this master's own
# commanded solar mode, with no readback. Fixed, spec-defined option list, not user-configurable.
SOLAR_MODE_OPTIONS = [
    "Off",
    "DHW Eco",
    "DHW Comfort",
    "DHW Single Boost",
    "DHW Continuous Boost",
]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_OPENTHERM42_ID): cv.use_id(OpenTherm42Hub),
        cv.Optional(
            CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_SOLAR_STORAGE_STATUS_SOLAR_MODE
        ): select.select_schema(
            OpenTherm42Select, entity_category=ENTITY_CATEGORY_CONFIG
        ).extend(
            {
                # Required, not merely defaulted: this is the value actually written to the boiler
                # on first boot / after a corrupt-or-missing preference, so it must be a deliberate
                # choice -- see OpenTherm42Select and the switch/number platforms' same rule.
                cv.Required(CONF_INITIAL_OPTION): cv.one_of(
                    *SOLAR_MODE_OPTIONS, string=True
                ),
            }
        ),
    }
)


async def to_code(config: dict) -> None:
    hub = await cg.get_variable(config[CONF_OPENTHERM42_ID])
    marker = CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_SOLAR_STORAGE_STATUS_SOLAR_MODE
    if (marker_config := config.get(marker)) is not None:
        var = await select.new_select(marker_config, options=SOLAR_MODE_OPTIONS)
        await cg.register_component(var, marker_config)
        cg.add(var.set_initial_option(marker_config[CONF_INITIAL_OPTION]))
        cg.add(getattr(hub, f"set_{marker}_select")(var))
