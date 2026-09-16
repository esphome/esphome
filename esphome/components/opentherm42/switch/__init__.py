import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_RESTORE_MODE, ENTITY_CATEGORY_CONFIG

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
    CONF_OPENTHERM42_ID,
)

OpenTherm42Switch = opentherm42_ns.class_(
    "OpenTherm42Switch", switch.Switch, cg.Component
)


def _switch_schema(*, entity_category: str = ENTITY_CATEGORY_CONFIG) -> cv.Schema:
    # restore_mode has no schema default here (unlike core switch platforms, which default to
    # ALWAYS_OFF): every write-only switch must state explicitly what it sends on first boot / after
    # a factory reset, since that's a real value pushed to the boiler, not just a UI toggle.
    # entity_category defaults to CONFIG, for switches that change control strategy/mode (how the
    # boiler operates) rather than issue a demand. Entries that ARE an active demand -- the same
    # nature as control_setpoint in the number platform -- override it to unset (primary). No
    # device_class default is set since none of them are power outlets (the only non-generic switch
    # device_class Home Assistant defines).
    return switch.switch_schema(
        OpenTherm42Switch, entity_category=entity_category
    ).extend(
        {
            cv.Required(CONF_RESTORE_MODE): cv.enum(
                switch.RESTORE_MODES, upper=True, space="_"
            ),
        }
    )


TYPES: dict[str, cv.Schema] = {
    # §5.3.1 Class 1, ID 0 HB bit 0: CH enable [ CH is disabled, CH is enabled ]. Gates whether
    # control_setpoint (number platform) has any effect -- an active demand, so primary.
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_CH_ENABLE: _switch_schema(
        entity_category=cv.UNDEFINED
    ),
    # §5.3.1 Class 1, ID 0 HB bit 1: DHW enable [ DHW is disabled, DHW is enabled ]. Enables DHW
    # production right now -- an active demand, so primary.
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_DHW_ENABLE: _switch_schema(
        entity_category=cv.UNDEFINED
    ),
    # §5.3.1 Class 1, ID 0 HB bit 2: Cooling enable [ Cooling is disabled, Cooling is enabled ].
    # Enables cooling right now -- an active demand, so primary.
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_COOLING_ENABLE: _switch_schema(
        entity_category=cv.UNDEFINED
    ),
    # §5.3.1 Class 1, ID 0 HB bit 3: OTC active [ OTC not active, OTC is active ]. Toggles how the
    # setpoint is calculated, not whether heating runs, so CONFIG (default).
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_OTC_ACTIVE: _switch_schema(),
    # §5.3.1 Class 1, ID 0 HB bit 4: CH2 enable [ CH2 is disabled, CH2 is enabled ]. Gates whether
    # control_setpoint_2 (number platform) has any effect -- an active demand, so primary.
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_CH2_ENABLE: _switch_schema(
        entity_category=cv.UNDEFINED
    ),
    # §5.3.1 Class 1, ID 0 HB bit 5: Summer/winter mode [ winter mode active, summer mode active ].
    # A standing seasonal mode, not a per-cycle demand, so CONFIG (default).
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_SUMMER_WINTER_MODE: _switch_schema(),
    # §5.3.1 Class 1, ID 0 HB bit 6: DHW blocking [ DHW unblocked, DHW blocked ]. Blocks DHW
    # production right now -- an active demand, so primary.
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_DHW_BLOCKING: _switch_schema(
        entity_category=cv.UNDEFINED
    ),
    # §5.3.1 Class 1, ID 70 HB bit 0: Ventilation enable [ disabled, enabled ]. Enables ventilation
    # right now -- an active demand, so primary.
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_FOR_VENTILATION_HEAT_RECOVERY_VENTILATION_ENABLE: _switch_schema(
        entity_category=cv.UNDEFINED
    ),
    # §5.3.1 Class 1, ID 70 HB bit 1: Bypass position (only bypass manual mode) [ close bypass, open
    # bypass ]. A literal physical actuator command -- an active demand, so primary.
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_FOR_VENTILATION_HEAT_RECOVERY_BYPASS_POSITION: _switch_schema(
        entity_category=cv.UNDEFINED
    ),
    # §5.3.1 Class 1, ID 70 HB bit 2: Bypass mode [ manual, automatic ]. Changes how bypass is
    # controlled, not whether it runs, so CONFIG (default) -- parallels OTC active above.
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_FOR_VENTILATION_HEAT_RECOVERY_BYPASS_MODE: _switch_schema(),
    # §5.3.1 Class 1, ID 70 HB bit 3: Free ventilation mode [ not active, active ]. Activates free
    # ventilation right now -- an active demand, so primary.
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_FOR_VENTILATION_HEAT_RECOVERY_FREE_VENTILATION_MODE: _switch_schema(
        entity_category=cv.UNDEFINED
    ),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_OPENTHERM42_ID): cv.use_id(OpenTherm42Hub),
        **{
            cv.Optional(marker): schema.extend(cv.COMPONENT_SCHEMA)
            for marker, schema in TYPES.items()
        },
    }
)


async def to_code(config: dict) -> None:
    hub = await cg.get_variable(config[CONF_OPENTHERM42_ID])
    for marker in TYPES:
        if (marker_config := config.get(marker)) is not None:
            var = await switch.new_switch(marker_config)
            await cg.register_component(var, marker_config)
            cg.add(getattr(hub, f"set_{marker}_switch")(var))
