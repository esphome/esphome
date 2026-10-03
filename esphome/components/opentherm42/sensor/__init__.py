import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_INDEX, DEVICE_CLASS_DURATION, ENTITY_CATEGORY_DIAGNOSTIC

from .. import OpenTherm42Hub
from ..const import (
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_DIAGNOSTIC_CODE,
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_DIAGNOSTIC_CODE_VENTILATION_HEAT_RECOVERY,
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_FAULT_CODE,
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_FAULT_CODE_SOLAR_STORAGE,
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_FAULT_CODE_VENTILATION_HEAT_RECOVERY,
    CONF_OPENTHERM42_ID,
    CONF_PASS_DURATION,
    CONF_SWEEP_DURATION,
    CONF_UPDATE_EVERY,
)


# All of these sensors are boiler-reported values: on a failed conversation, every configured sensor
# here must show unknown rather than keep a stale reading.
#
# entity_category follows the data's own nature, independent of which spec class it lives in:
# - Live, changeable status/fault/mode readouts and setpoints the boiler is currently operating on
#   are primary (entity_category left unset).
# - Static identification/capability values the boiler reports once (member IDs, product
#   type/version, protocol version, TSP/fault-history-buffer counts, installation-time
#   bounds/limits) are DIAGNOSTIC -- Home Assistant's own example of a diagnostic entity is "a
#   sensor showing... MAC address", exactly this kind of read-only identifier.
# - Lifetime wear/reliability counters for an internal moving part (burner ignitions, pump starts,
#   unsuccessful starts, flame-signal faults, power cycles) are DIAGNOSTIC -- used for service
#   scheduling and troubleshooting, not day-to-day monitoring, unlike genuine usage/production
#   statistics (electricity produced, cooling hours) which stay primary.
#
# No sensor here gets a state_class of "measurement" for a discrete code (fault code, mode, product
# type/version): averaging or graphing a category number is meaningless, so codes get no state_class
# at all, the same as Home Assistant's own convention for identifier-like sensors.
def _code_schema(*, entity_category: str = cv.UNDEFINED) -> cv.Schema:
    return sensor.sensor_schema(accuracy_decimals=0, entity_category=entity_category)


# §5.1: OpenTherm protocol versions are f8.8 (e.g. 2.2, 4.2) -- two decimals is enough to show them exactly.
def _version_schema(*, entity_category: str = cv.UNDEFINED) -> cv.Schema:
    return sensor.sensor_schema(accuracy_decimals=2, entity_category=entity_category)


def _temperature_schema(
    *, accuracy_decimals: int = 2, entity_category: str = cv.UNDEFINED
) -> cv.Schema:
    return sensor.sensor_schema(
        unit_of_measurement="°C",
        accuracy_decimals=accuracy_decimals,
        device_class="temperature",
        state_class="measurement",
        entity_category=entity_category,
    )


def _percent_schema(
    *, accuracy_decimals: int = 1, entity_category: str = cv.UNDEFINED
) -> cv.Schema:
    return sensor.sensor_schema(
        unit_of_measurement="%",
        accuracy_decimals=accuracy_decimals,
        state_class="measurement",
        entity_category=entity_category,
    )


def _count_schema(*, entity_category: str = cv.UNDEFINED) -> cv.Schema:
    return sensor.sensor_schema(
        accuracy_decimals=0,
        state_class="total_increasing",
        entity_category=entity_category,
    )


def _hours_schema(*, entity_category: str = cv.UNDEFINED) -> cv.Schema:
    return sensor.sensor_schema(
        unit_of_measurement="h",
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_DURATION,
        state_class="total_increasing",
        entity_category=entity_category,
    )


_CODE_SCHEMA = _code_schema()
_TEMPERATURE_SCHEMA = _temperature_schema()
_PERCENT_SCHEMA = _percent_schema()
_COUNT_SCHEMA = _count_schema()
_HOURS_SCHEMA = _hours_schema()


# Only for 1:1 ids (one sensor per conversation) -- ids that share a conversation with other
# entities get their update_every from a hub-level option instead (see opentherm42/__init__.py's
# UPDATE_EVERY_OPTIONS), so this must never be applied to a schema shared with one of those (e.g.
# _percent_schema() is also used by the 1:N MINIMUM_MODULATION_LEVEL sensor -- only wrap the
# specific 1:1 dict entries below, never a shared _*_SCHEMA constant itself). `default` is this
# id's priority tier -- see opentherm42/__init__.py's UPDATE_EVERY_OPTIONS for the scheme.
def _with_update_every(schema: cv.Schema, default: int) -> cv.Schema:
    return schema.extend(
        {
            cv.Optional(CONF_UPDATE_EVERY, default=default): cv.int_range(min=1),
        }
    )


TYPES: dict[str, cv.Schema] = {
    # §5.3.1 Class 1, ID 5 LB: OEM fault code (0..255) -- an OEM-specific fault/error code.
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_FAULT_CODE: _CODE_SCHEMA,
    # §5.3.1 Class 1, ID 72 LB: OEM fault code ventilation/heat-recovery (0..255).
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_FAULT_CODE_VENTILATION_HEAT_RECOVERY: _CODE_SCHEMA,
    # §5.3.1 Class 1, ID 102 LB: OEM fault code Solar Storage (0..255). 1:1 (HB is entirely
    # reserved, no other entity shares this conversation), unlike OEM_FAULT_CODE/
    # _VENTILATION_HEAT_RECOVERY above, which are part of the FAULT_FLAGS/VENTILATION_FAULT_FLAGS
    # groups -- see opentherm42/__init__.py's UPDATE_EVERY_OPTIONS.
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_FAULT_CODE_SOLAR_STORAGE: _with_update_every(
        _CODE_SCHEMA, 5
    ),
    # §5.3.1 Class 1, ID 115: OEM diagnostic code (0..65535) -- an OEM-specific diagnostic/service code.
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_DIAGNOSTIC_CODE: _with_update_every(
        _CODE_SCHEMA, 5
    ),
    # §5.3.1 Class 1, ID 73: OEM diagnostic code ventilation/heat-recovery (0..65535).
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_DIAGNOSTIC_CODE_VENTILATION_HEAT_RECOVERY: _with_update_every(
        _CODE_SCHEMA, 5
    ),
    # Synthetic diagnostic, not an OpenTherm data-id -- deliberately not given the
    # sensor_and_informational_data_* prefix used throughout this file, since that names a real spec
    # chapter (§5.3.4 Class 4) this entity has nothing to do with. How long the most recently
    # completed sweep (a full pass attempting every configured id at least once) took, updated
    # continuously at the end of every sweep -- see hub.h's Entry/sweep_length_passes_ for the
    # pass-based scheduler this measures. Unconditionally scheduled by the hub itself regardless of
    # what else is configured, so no update_every field of its own.
    CONF_SWEEP_DURATION: sensor.sensor_schema(
        unit_of_measurement="ms",
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_DURATION,
        state_class="measurement",
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    # Same nature as the sweep duration sensor above, but for a single pass (one full scan of every
    # configured id, however many of them happened to be due on it) rather than a whole sweep --
    # updated continuously at the end of every pass. Also unconditionally scheduled by the hub
    # itself, so no update_every field of its own.
    CONF_PASS_DURATION: sensor.sensor_schema(
        unit_of_measurement="ms",
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_DURATION,
        state_class="measurement",
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}

# Every 1:1 id above dispatched through hub.cpp's generic SIMPLE_SENSORS table -- update_every
# for these routes through the single generic hub.set_simple_sensor_update_every(id, n), keyed
# by data-id rather than one named setter per marker (unlike every other platform here). Excludes
# OEM_DIAGNOSTIC_CODE/_VENTILATION_HEAT_RECOVERY, which have bespoke (non-SIMPLE_SENSORS) handling
# in hub.cpp and so get their own individually-named setters instead -- see to_code() below.
SIMPLE_SENSOR_DATA_IDS: dict[str, int] = {}

# §5.3.7 Class 7, IDs 13/91/108: one list of user-named fault-history-buffer slots per family, keyed
# by which data-id reads that family's fault history.
FHB_FAMILY_DATA_IDS: dict[str, int] = {}

# Stored fault-history entries are diagnostic/troubleshooting information by nature, matching Home
# Assistant's own example of a diagnostic entity.
FHB_ENTRY_SCHEMA = _code_schema(entity_category=ENTITY_CATEGORY_DIAGNOSTIC).extend(
    {
        # FHB-index: which of the boiler's (opaque, manufacturer-specific) fault history entries this
        # slot reads.
        cv.Required(CONF_INDEX): cv.int_range(min=0, max=255),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_OPENTHERM42_ID): cv.use_id(OpenTherm42Hub),
        **{cv.Optional(marker): schema for marker, schema in TYPES.items()},
        **{
            cv.Optional(marker, default=[]): cv.ensure_list(FHB_ENTRY_SCHEMA)
            for marker in FHB_FAMILY_DATA_IDS
        },
    }
)


async def to_code(config: dict) -> None:
    hub = await cg.get_variable(config[CONF_OPENTHERM42_ID])
    for marker in TYPES:
        if (marker_config := config.get(marker)) is not None:
            var = await sensor.new_sensor(marker_config)
            cg.add(getattr(hub, f"set_{marker}_sensor")(var))
            if (update_every := marker_config.get(CONF_UPDATE_EVERY)) is not None:
                if marker in SIMPLE_SENSOR_DATA_IDS:
                    cg.add(
                        hub.set_simple_sensor_update_every(
                            SIMPLE_SENSOR_DATA_IDS[marker], update_every
                        )
                    )
                else:
                    # OEM_DIAGNOSTIC_CODE/_VENTILATION_HEAT_RECOVERY and OEM_FAULT_CODE_SOLAR_STORAGE --
                    # bespoke (non-SIMPLE_SENSORS) handling in hub.cpp, so each gets its own
                    # individually-named setter instead.
                    cg.add(getattr(hub, f"set_{marker}_update_every")(update_every))

    for marker, data_id in FHB_FAMILY_DATA_IDS.items():
        for entry in config[marker]:
            var = await sensor.new_sensor(entry)
            cg.add(hub.add_fhb_slot(data_id, entry[CONF_INDEX], var))
