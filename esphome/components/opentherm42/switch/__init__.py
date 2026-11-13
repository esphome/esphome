import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_RESTORE_MODE

from .. import OpenTherm42Hub, opentherm42_ns
from ..const import (
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_CH2_ENABLE,
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_CH_ENABLE,
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_COOLING_ENABLE,
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_DHW_BLOCKING,
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_DHW_ENABLE,
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_FOR_VENTILATION_HEAT_RECOVERY_BYPASS_MODE,
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_FOR_VENTILATION_HEAT_RECOVERY_BYPASS_POSITION,
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_FOR_VENTILATION_HEAT_RECOVERY_FREE_VENTILATION_MODE,
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_FOR_VENTILATION_HEAT_RECOVERY_VENTILATION_ENABLE,
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_OTC_ACTIVE,
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_SUMMER_WINTER_MODE,
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_MANUAL_DHW_PUSH2,
    CONF_OPENTHERM42_ID,
)

OpenTherm42Switch = opentherm42_ns.class_(
    "OpenTherm42Switch", switch.Switch, cg.Component
)
OpenTherm42ManualDhwPush2Switch = opentherm42_ns.class_(
    "OpenTherm42ManualDhwPush2Switch", switch.Switch, cg.Component
)


def _switch_schema() -> cv.Schema:
    # restore_mode has no schema default here (unlike core switch platforms, which default to
    # ALWAYS_OFF): every write-only switch must state explicitly what it sends on first boot / after
    # a factory reset, since that's a real value pushed to the boiler, not just a UI toggle.
    # entity_category is left unset (primary): every one of these switches is an active demand sent
    # to the boiler right now -- this integration's main purpose, same nature as control_setpoint in
    # the number platform -- not a secondary/installation-time tuning parameter. No device_class
    # default is set since none of them are power outlets (the only non-generic switch device_class
    # Home Assistant defines).
    return switch.switch_schema(OpenTherm42Switch).extend(
        {
            cv.Required(CONF_RESTORE_MODE): cv.enum(
                switch.RESTORE_MODES, upper=True, space="_"
            ),
        }
    )


TYPES: dict[str, cv.Schema] = {
    # §5.3.1 Class 1, ID 0 HB bit 0: CH enable [ CH is disabled, CH is enabled ]
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_CH_ENABLE: _switch_schema(),
    # §5.3.1 Class 1, ID 0 HB bit 1: DHW enable [ DHW is disabled, DHW is enabled ]
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_DHW_ENABLE: _switch_schema(),
    # §5.3.1 Class 1, ID 0 HB bit 2: Cooling enable [ Cooling is disabled, Cooling is enabled ]
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_COOLING_ENABLE: _switch_schema(),
    # §5.3.1 Class 1, ID 0 HB bit 3: OTC active [ OTC not active, OTC is active ]
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_OTC_ACTIVE: _switch_schema(),
    # §5.3.1 Class 1, ID 0 HB bit 4: CH2 enable [ CH2 is disabled, CH2 is enabled ]
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_CH2_ENABLE: _switch_schema(),
    # §5.3.1 Class 1, ID 0 HB bit 5: Summer/winter mode [ winter mode active, summer mode active ]
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_SUMMER_WINTER_MODE: _switch_schema(),
    # §5.3.1 Class 1, ID 0 HB bit 6: DHW blocking [ DHW unblocked, DHW blocked ]
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_DHW_BLOCKING: _switch_schema(),
    # §5.3.1 Class 1, ID 70 HB bit 0: Ventilation enable [ disabled, enabled ]
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_FOR_VENTILATION_HEAT_RECOVERY_VENTILATION_ENABLE: _switch_schema(),
    # §5.3.1 Class 1, ID 70 HB bit 1: Bypass position (only bypass manual mode) [ close bypass, open bypass ]
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_FOR_VENTILATION_HEAT_RECOVERY_BYPASS_POSITION: _switch_schema(),
    # §5.3.1 Class 1, ID 70 HB bit 2: Bypass mode [ manual, automatic ]
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_FOR_VENTILATION_HEAT_RECOVERY_BYPASS_MODE: _switch_schema(),
    # §5.3.1 Class 1, ID 70 HB bit 3: Free ventilation mode [ not active, active ]
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_FOR_VENTILATION_HEAT_RECOVERY_FREE_VENTILATION_MODE: _switch_schema(),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_OPENTHERM42_ID): cv.use_id(OpenTherm42Hub),
        **{
            cv.Optional(marker): schema.extend(cv.COMPONENT_SCHEMA)
            for marker, schema in TYPES.items()
        },
        # §5.3.8.3 Class 8, ID 99 HB bit 4: Manual DHW push2 -- read/write, packed alongside the three
        # Operating Mode selects (see the select platform). Populated only from the periodic read (see
        # hub.cpp's REMOTE_OVERRIDE_OPERATING_MODES_READ case), never from a write-ack echo, so unlike
        # every switch above there's no restore_mode: it starts at the safe "no push" default until
        # that first real read arrives. Left as a primary entity (no entity_category): a "boost my hot
        # water now" action pressed as part of normal use, not a maintenance/admin action.
        cv.Optional(
            CONF_CONTROL_OF_SPECIAL_APPLICATIONS_MANUAL_DHW_PUSH2
        ): switch.switch_schema(OpenTherm42ManualDhwPush2Switch).extend(
            cv.COMPONENT_SCHEMA
        ),
    }
)


async def to_code(config: dict) -> None:
    hub = await cg.get_variable(config[CONF_OPENTHERM42_ID])
    for marker in TYPES:
        if (marker_config := config.get(marker)) is not None:
            var = await switch.new_switch(marker_config)
            await cg.register_component(var, marker_config)
            cg.add(getattr(hub, f"set_{marker}_switch")(var))

    marker = CONF_CONTROL_OF_SPECIAL_APPLICATIONS_MANUAL_DHW_PUSH2
    if (marker_config := config.get(marker)) is not None:
        var = await switch.new_switch(marker_config)
        await cg.register_component(var, marker_config)
        cg.add(getattr(hub, f"set_{marker}_switch")(var))
