import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv

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

# Every switch defaults its restore_mode to RESTORE_DEFAULT_OFF: on first boot (no persisted state)
# the bit is sent as clear/0, the safest default for a control bit the boiler will act on.
TYPES: dict[str, cv.Schema] = {
    # §5.3.1 Class 1, ID 0 HB bit 0: CH enable [ CH is disabled, CH is enabled ]
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_CH_ENABLE: switch.switch_schema(
        OpenTherm42Switch, default_restore_mode="RESTORE_DEFAULT_OFF"
    ),
    # §5.3.1 Class 1, ID 0 HB bit 1: DHW enable [ DHW is disabled, DHW is enabled ]
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_DHW_ENABLE: switch.switch_schema(
        OpenTherm42Switch, default_restore_mode="RESTORE_DEFAULT_OFF"
    ),
    # §5.3.1 Class 1, ID 0 HB bit 2: Cooling enable [ Cooling is disabled, Cooling is enabled ]
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_COOLING_ENABLE: switch.switch_schema(
        OpenTherm42Switch, default_restore_mode="RESTORE_DEFAULT_OFF"
    ),
    # §5.3.1 Class 1, ID 0 HB bit 3: OTC active [ OTC not active, OTC is active ]
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_OTC_ACTIVE: switch.switch_schema(
        OpenTherm42Switch, default_restore_mode="RESTORE_DEFAULT_OFF"
    ),
    # §5.3.1 Class 1, ID 0 HB bit 4: CH2 enable [ CH2 is disabled, CH2 is enabled ]
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_CH2_ENABLE: switch.switch_schema(
        OpenTherm42Switch, default_restore_mode="RESTORE_DEFAULT_OFF"
    ),
    # §5.3.1 Class 1, ID 0 HB bit 5: Summer/winter mode [ winter mode active, summer mode active ]
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_SUMMER_WINTER_MODE: switch.switch_schema(
        OpenTherm42Switch, default_restore_mode="RESTORE_DEFAULT_OFF"
    ),
    # §5.3.1 Class 1, ID 0 HB bit 6: DHW blocking [ DHW unblocked, DHW blocked ]
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_DHW_BLOCKING: switch.switch_schema(
        OpenTherm42Switch, default_restore_mode="RESTORE_DEFAULT_OFF"
    ),
    # §5.3.1 Class 1, ID 70 HB bit 0: Ventilation enable [ disabled, enabled ]
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_FOR_VENTILATION_HEAT_RECOVERY_VENTILATION_ENABLE: switch.switch_schema(
        OpenTherm42Switch, default_restore_mode="RESTORE_DEFAULT_OFF"
    ),
    # §5.3.1 Class 1, ID 70 HB bit 1: Bypass position (only bypass manual mode) [ close bypass, open bypass ]
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_FOR_VENTILATION_HEAT_RECOVERY_BYPASS_POSITION: switch.switch_schema(
        OpenTherm42Switch, default_restore_mode="RESTORE_DEFAULT_OFF"
    ),
    # §5.3.1 Class 1, ID 70 HB bit 2: Bypass mode [ manual, automatic ]
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_FOR_VENTILATION_HEAT_RECOVERY_BYPASS_MODE: switch.switch_schema(
        OpenTherm42Switch, default_restore_mode="RESTORE_DEFAULT_OFF"
    ),
    # §5.3.1 Class 1, ID 70 HB bit 3: Free ventilation mode [ not active, active ]
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_FOR_VENTILATION_HEAT_RECOVERY_FREE_VENTILATION_MODE: switch.switch_schema(
        OpenTherm42Switch, default_restore_mode="RESTORE_DEFAULT_OFF"
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
