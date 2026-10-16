import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import CONF_INITIAL_OPTION

from .. import OpenTherm42Hub, opentherm42_ns
from ..const import (
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_SOLAR_STORAGE_STATUS_SOLAR_MODE,
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_REMOTE_OVERRIDE_OPERATING_MODE_DHW,
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_REMOTE_OVERRIDE_OPERATING_MODE_HEATING_HC1,
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_REMOTE_OVERRIDE_OPERATING_MODE_HEATING_HC2,
    CONF_OPENTHERM42_ID,
)

OpenTherm42Select = opentherm42_ns.class_(
    "OpenTherm42Select", select.Select, cg.Component
)
OpenTherm42RemoteOverrideModeSelect = opentherm42_ns.class_(
    "OpenTherm42RemoteOverrideModeSelect", select.Select, cg.Component
)

# §5.3.1 Class 1, ID 101 HB bits 2,1,0: Master Solar Storage status: Solar mode -- master-authored,
# same "R -" idiom as ID 0/70's master status (see hub.h's RequestKind::SOLAR_STORAGE_STATUS
# comment): the wire message type is always READ-DATA, but the byte's content is this master's own
# commanded solar mode, with no readback. Fixed, spec-defined option list, not user-configurable.
# entity_category is left unset (primary): this is chosen situationally, same as an active demand,
# the same nature as the switch platform's Class 1 enable/demand entries.
SOLAR_MODE_OPTIONS = [
    "Off",
    "DHW Eco",
    "DHW Comfort",
    "DHW Single Boost",
    "DHW Continuous Boost",
]

# §5.3.8.3 Class 8, ID 99 LB bits 0-3/4-7: Operating Mode HC1/HC2 -- same 7-state enum shared by both
# heating zones.
HEATING_OPERATING_MODE_OPTIONS = [
    "No Override",
    "Auto",
    "Comfort",
    "Precomfort",
    "Reduced",
    "Protection",
    "Off",
]

# §5.3.8.3 Class 8, ID 99 HB bits 0-3: Operating Mode DHW -- its own distinct 7-state enum (Comfort
# is state 2 for HC1/HC2 above; here it's Anti-Legionella, and there's no Precomfort at all).
DHW_OPERATING_MODE_OPTIONS = [
    "No Override",
    "Auto",
    "Anti-Legionella",
    "Comfort",
    "Reduced",
    "Protection",
    "Off",
]

# One (marker, options, hub setter name) triple per Operating Mode nibble of ID 99 -- read/write,
# populated only from the periodic read (see hub.cpp's REMOTE_OVERRIDE_OPERATING_MODES_READ case),
# never from a write-ack echo, so unlike the solar mode select above there's no initial_option: it
# starts Unknown until that first real read.
REMOTE_OVERRIDE_MODE_SELECTS = (
    (
        CONF_CONTROL_OF_SPECIAL_APPLICATIONS_REMOTE_OVERRIDE_OPERATING_MODE_HEATING_HC1,
        HEATING_OPERATING_MODE_OPTIONS,
    ),
    (
        CONF_CONTROL_OF_SPECIAL_APPLICATIONS_REMOTE_OVERRIDE_OPERATING_MODE_HEATING_HC2,
        HEATING_OPERATING_MODE_OPTIONS,
    ),
    (
        CONF_CONTROL_OF_SPECIAL_APPLICATIONS_REMOTE_OVERRIDE_OPERATING_MODE_DHW,
        DHW_OPERATING_MODE_OPTIONS,
    ),
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_OPENTHERM42_ID): cv.use_id(OpenTherm42Hub),
        cv.Optional(
            CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_SOLAR_STORAGE_STATUS_SOLAR_MODE
        ): select.select_schema(OpenTherm42Select).extend(
            {
                # Required, not merely defaulted: this is the value actually written to the boiler
                # on first boot / after a corrupt-or-missing preference, so it must be a deliberate
                # choice -- see OpenTherm42Select and the switch/number platforms' same rule.
                cv.Required(CONF_INITIAL_OPTION): cv.one_of(
                    *SOLAR_MODE_OPTIONS, string=True
                ),
            }
        ),
        **{
            cv.Optional(marker): select.select_schema(
                OpenTherm42RemoteOverrideModeSelect
            )
            for marker, _options in REMOTE_OVERRIDE_MODE_SELECTS
        },
    }
)


async def to_code(config: dict) -> None:
    hub = await cg.get_variable(config[CONF_OPENTHERM42_ID])
    marker = CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_SOLAR_STORAGE_STATUS_SOLAR_MODE
    if (marker_config := config.get(marker)) is not None:
        var = await select.new_select(marker_config, hub, options=SOLAR_MODE_OPTIONS)
        await cg.register_component(var, marker_config)
        cg.add(var.set_initial_option(marker_config[CONF_INITIAL_OPTION]))
        cg.add(getattr(hub, f"set_{marker}_select")(var))

    for marker, options in REMOTE_OVERRIDE_MODE_SELECTS:
        if (marker_config := config.get(marker)) is not None:
            var = await select.new_select(marker_config, options=options)
            await cg.register_component(var, marker_config)
            cg.add(getattr(hub, f"set_{marker}_select")(var))
