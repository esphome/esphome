import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv

from .. import OpenTherm42Hub
from ..const import (
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_SOLAR_STORAGE_STATUS_SOLAR_MODE,
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_DIAGNOSTIC_CODE,
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_DIAGNOSTIC_CODE_VENTILATION_HEAT_RECOVERY,
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_FAULT_CODE,
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_FAULT_CODE_SOLAR_STORAGE,
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_FAULT_CODE_VENTILATION_HEAT_RECOVERY,
    CONF_CONTROL_AND_STATUS_INFORMATION_SOLAR_STORAGE_MODE_AND_STATUS_SOLAR_MODE,
    CONF_CONTROL_AND_STATUS_INFORMATION_SOLAR_STORAGE_MODE_AND_STATUS_SOLAR_STATUS,
)

CONF_OPENTHERM42_ID = "opentherm42_id"

# All of Class 1's sensors are boiler-reported codes: on a failed conversation, every configured
# sensor here must show unknown rather than keep a stale reading.
_CODE_SCHEMA = sensor.sensor_schema(accuracy_decimals=0)

TYPES: dict[str, cv.Schema] = {
    # §5.3.1 Class 1, ID 5 LB: OEM fault code (0..255) -- an OEM-specific fault/error code.
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_FAULT_CODE: _CODE_SCHEMA,
    # §5.3.1 Class 1, ID 72 LB: OEM fault code ventilation/heat-recovery (0..255).
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_FAULT_CODE_VENTILATION_HEAT_RECOVERY: _CODE_SCHEMA,
    # §5.3.1 Class 1, ID 102 LB: OEM fault code Solar Storage (0..255).
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_FAULT_CODE_SOLAR_STORAGE: _CODE_SCHEMA,
    # §5.3.1 Class 1, ID 115: OEM diagnostic code (0..65535) -- an OEM-specific diagnostic/service code.
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_DIAGNOSTIC_CODE: _CODE_SCHEMA,
    # §5.3.1 Class 1, ID 73: OEM diagnostic code ventilation/heat-recovery (0..65535).
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_DIAGNOSTIC_CODE_VENTILATION_HEAT_RECOVERY: _CODE_SCHEMA,
    # §5.3.1 Class 1, ID 101 HB bits 2,1,0: Master Solar Storage status: Solar mode.
    # 0=off, 1=DHW eco, 2=DHW comfort, 3=DHW single boost, 4=DHW continuous boost.
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_SOLAR_STORAGE_STATUS_SOLAR_MODE: _CODE_SCHEMA,
    # §5.3.1 Class 1, ID 101 LB bits 3,2,1: Solar Storage mode and status: Solar mode.
    # 0=off, 1=DHW eco, 2=DHW comfort, 3=DHW single boost, 4=DHW continuous boost.
    CONF_CONTROL_AND_STATUS_INFORMATION_SOLAR_STORAGE_MODE_AND_STATUS_SOLAR_MODE: _CODE_SCHEMA,
    # §5.3.1 Class 1, ID 101 LB bits 5,4: Solar Storage mode and status: Solar status.
    # 0=standby, 1=loading of solar storage tank by the sun, 2=loading by the boiler, 3=anti-legionella mode active.
    CONF_CONTROL_AND_STATUS_INFORMATION_SOLAR_STORAGE_MODE_AND_STATUS_SOLAR_STATUS: _CODE_SCHEMA,
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_OPENTHERM42_ID): cv.use_id(OpenTherm42Hub),
        **{cv.Optional(marker): schema for marker, schema in TYPES.items()},
    }
)


async def to_code(config: dict) -> None:
    hub = await cg.get_variable(config[CONF_OPENTHERM42_ID])
    for marker in TYPES:
        if (marker_config := config.get(marker)) is not None:
            var = await sensor.new_sensor(marker_config)
            cg.add(getattr(hub, f"set_{marker}_sensor")(var))
