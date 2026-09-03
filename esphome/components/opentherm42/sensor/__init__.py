import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv

from .. import OpenTherm42Hub
from ..const import (
    CONF_CONFIGURATION_INFORMATION_BOILER_MEMBER_ID_CODE,
    CONF_CONFIGURATION_INFORMATION_BOILER_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_TYPE,
    CONF_CONFIGURATION_INFORMATION_BOILER_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_VERSION,
    CONF_CONFIGURATION_INFORMATION_MEMBER_ID_CODE_VENTILATION_HEAT_RECOVERY,
    CONF_CONFIGURATION_INFORMATION_OPENTHERM_VERSION_BOILER,
    CONF_CONFIGURATION_INFORMATION_OPENTHERM_VERSION_VENTILATION_HEAT_RECOVERY,
    CONF_CONFIGURATION_INFORMATION_SOLAR_STORAGE_MEMBER_ID,
    CONF_CONFIGURATION_INFORMATION_SOLAR_STORAGE_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_TYPE,
    CONF_CONFIGURATION_INFORMATION_SOLAR_STORAGE_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_VERSION,
    CONF_CONFIGURATION_INFORMATION_VENTILATION_HEAT_RECOVERY_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_TYPE,
    CONF_CONFIGURATION_INFORMATION_VENTILATION_HEAT_RECOVERY_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_VERSION,
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_SOLAR_STORAGE_STATUS_SOLAR_MODE,
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_DIAGNOSTIC_CODE,
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_DIAGNOSTIC_CODE_VENTILATION_HEAT_RECOVERY,
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_FAULT_CODE,
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_FAULT_CODE_SOLAR_STORAGE,
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_FAULT_CODE_VENTILATION_HEAT_RECOVERY,
    CONF_CONTROL_AND_STATUS_INFORMATION_SOLAR_STORAGE_MODE_AND_STATUS_SOLAR_MODE,
    CONF_CONTROL_AND_STATUS_INFORMATION_SOLAR_STORAGE_MODE_AND_STATUS_SOLAR_STATUS,
    CONF_REMOTE_REQUEST_LAST_RESPONSE_CODE,
)

CONF_OPENTHERM42_ID = "opentherm42_id"

# All of these sensors are boiler-reported codes: on a failed conversation, every configured sensor
# here must show unknown rather than keep a stale reading.
_CODE_SCHEMA = sensor.sensor_schema(accuracy_decimals=0)
# §5.1: OpenTherm protocol versions are f8.8 (e.g. 2.2, 4.2) -- two decimals is enough to show them exactly.
_VERSION_SCHEMA = sensor.sensor_schema(accuracy_decimals=2)

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
    # §5.3.2 Class 2, ID 3 LB: Boiler MemberID code (0..255) -- identifies the boiler's manufacturer.
    CONF_CONFIGURATION_INFORMATION_BOILER_MEMBER_ID_CODE: _CODE_SCHEMA,
    # §5.3.2 Class 2, ID 125: OpenTherm protocol version implemented by the boiler.
    CONF_CONFIGURATION_INFORMATION_OPENTHERM_VERSION_BOILER: _VERSION_SCHEMA,
    # §5.3.2 Class 2, ID 127 HB: Boiler product version number and type: product type (0..255).
    CONF_CONFIGURATION_INFORMATION_BOILER_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_TYPE: _CODE_SCHEMA,
    # §5.3.2 Class 2, ID 127 LB: Boiler product version number and type: product version (0..255).
    CONF_CONFIGURATION_INFORMATION_BOILER_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_VERSION: _CODE_SCHEMA,
    # §5.3.2 Class 2, ID 74 LB: MemberID code ventilation/heat-recovery (0..255).
    CONF_CONFIGURATION_INFORMATION_MEMBER_ID_CODE_VENTILATION_HEAT_RECOVERY: _CODE_SCHEMA,
    # §5.3.2 Class 2, ID 75: OpenTherm protocol version implemented by the ventilation/heat-recovery system.
    CONF_CONFIGURATION_INFORMATION_OPENTHERM_VERSION_VENTILATION_HEAT_RECOVERY: _VERSION_SCHEMA,
    # §5.3.2 Class 2, ID 76 HB: Ventilation/heat-recovery product version number and type: product type.
    CONF_CONFIGURATION_INFORMATION_VENTILATION_HEAT_RECOVERY_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_TYPE: (
        _CODE_SCHEMA
    ),
    # §5.3.2 Class 2, ID 76 LB: Ventilation/heat-recovery product version number and type: product version.
    CONF_CONFIGURATION_INFORMATION_VENTILATION_HEAT_RECOVERY_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_VERSION: (
        _CODE_SCHEMA
    ),
    # §5.3.2 Class 2, ID 103 LB: Solar Storage member ID (0..255).
    CONF_CONFIGURATION_INFORMATION_SOLAR_STORAGE_MEMBER_ID: _CODE_SCHEMA,
    # §5.3.2 Class 2, ID 104 HB: Solar Storage product version number and type: product type (0..255).
    CONF_CONFIGURATION_INFORMATION_SOLAR_STORAGE_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_TYPE: _CODE_SCHEMA,
    # §5.3.2 Class 2, ID 104 LB: Solar Storage product version number and type: product version (0..255).
    CONF_CONFIGURATION_INFORMATION_SOLAR_STORAGE_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_VERSION: _CODE_SCHEMA,
    # §5.3.3 Class 3, ID 4 LB: Req-Response-Code of the most recent remote request
    # (0..127 = request refused, 128..255 = request accepted).
    CONF_REMOTE_REQUEST_LAST_RESPONSE_CODE: _CODE_SCHEMA,
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
