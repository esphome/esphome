import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_INDEX,
    CONF_INITIAL_VALUE,
    CONF_NUMBER,
    CONF_PLATFORM,
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_CONFIG,
)
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import OpenTherm42Hub, opentherm42_ns
from ..const import (
    CONF_CONTROL_AND_STATUS_INFORMATION_CONTROL_SETPOINT,
    CONF_OPENTHERM42_ID,
    CONF_UPDATE_EVERY,
)

OpenTherm42Number = opentherm42_ns.class_(
    "OpenTherm42Number", number.Number, cg.Component
)
OpenTherm42SensorFeedNumber = opentherm42_ns.class_(
    "OpenTherm42SensorFeedNumber", number.Number, cg.Component
)
OpenTherm42TspNumber = opentherm42_ns.class_(
    "OpenTherm42TspNumber", number.Number, cg.Component
)


def _number_schema(
    unit_of_measurement: str,
    min_value: float,
    max_value: float,
    default_update_every: int,
    *,
    device_class: str = cv.UNDEFINED,
    entity_category: str = cv.UNDEFINED,
) -> cv.Schema:
    # initial_value is required, not merely defaulted: this is the value actually written to the
    # boiler on first boot / after a corrupt-or-missing preference (see OpenTherm42Number), so it must
    # be a deliberate choice, not a value picked by this component on the user's behalf.
    return number.number_schema(
        OpenTherm42Number,
        unit_of_measurement=unit_of_measurement,
        device_class=device_class,
        entity_category=entity_category,
    ).extend(
        {
            cv.Required(CONF_INITIAL_VALUE): cv.float_range(
                min=min_value, max=max_value
            ),
            # For the three R/W pairs (ids 56/57/87), this governs the READ side's cadence, not the
            # write side's -- see hub.h's set_number_update_every(). default_update_every is this
            # id's priority tier (see opentherm42/__init__.py's UPDATE_EVERY_OPTIONS for the scheme).
            cv.Optional(CONF_UPDATE_EVERY, default=default_update_every): cv.int_range(
                min=1
            ),
        }
    )


# §5.1: the master decides the actual min/max/step of the value it sends -- the spec only defines
# the wire range. entity_category is left unset (primary) for the setpoints that are this
# integration's main purpose -- telling the boiler/ventilation/cooling plant what to do right now.
# It's set to CONFIG for values that either (a) forward this master's own external sensor reading
# into the boiler's control loop rather than expressing a demand, or (b) are installation-time
# limits/tuning parameters that aren't part of day-to-day operation. Keyed by marker -> (schema,
# number.new_number traits, OpenTherm data-id -- OpenTherm42Number's constructor needs it to route
# control()'s value to the right internal field on the hub, see hub.h's set_write_value()).
TYPES: dict[str, tuple[cv.Schema, dict, int]] = {
    # §5.3.1 Class 1, ID 1: Control Setpoint, i.e. CH water temperature setpoint (degrees C, 0..100).
    # The CHenable bit (see switch platform) has priority: the boiler must ignore this value while
    # CH is disabled.
    CONF_CONTROL_AND_STATUS_INFORMATION_CONTROL_SETPOINT: (
        _number_schema("°C", 0, 100, 1, device_class=DEVICE_CLASS_TEMPERATURE),
        {"min_value": 0, "max_value": 100, "step": 0.1},
        1,
    ),
}

# §5.3.4 Class 4, IDs 24/37 and IDs 27/38/78/79: this master's own external sensor readings, pushed to the
# boiler. Unlike TYPES above, these take no initial_value -- there's nothing for this component to
# invent a default for (see OpenTherm42SensorFeedNumber's class comment) -- and the constructor also
# needs the OpenTherm data-id, which the hub uses to route control()'s value to the right internal
# field (see hub.h's set_sensor_feed_write_value()). Keyed by marker -> (schema, number.new_number
# traits, id).
SENSOR_FEED_TYPES: dict[str, tuple[cv.Schema, dict, int]] = {}

# §5.3.6 Class 6, IDs 11/89/106: one list of user-named TSP slots per family, keyed by which data-id
# reads/writes that family's transparent-boiler-parameters.
TSP_FAMILY_DATA_IDS: dict[str, int] = {}

TSP_ENTRY_SCHEMA = (
    # Raw, opaque, manufacturer-specific parameter access -- an advanced/expert interface, not a
    # value anyone adjusts as part of normal operation, so CONFIG.
    number.number_schema(OpenTherm42TspNumber, entity_category=ENTITY_CATEGORY_CONFIG)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(
        {
            # TSP-index: which of the boiler's (opaque, manufacturer-specific) parameters this slot reads/writes.
            cv.Required(CONF_INDEX): cv.int_range(min=0, max=255),
        }
    )
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_OPENTHERM42_ID): cv.use_id(OpenTherm42Hub),
        **{
            cv.Optional(marker): schema.extend(cv.COMPONENT_SCHEMA)
            for marker, (schema, _traits, _id) in TYPES.items()
        },
        **{
            cv.Optional(marker): schema.extend(cv.COMPONENT_SCHEMA)
            for marker, (schema, _traits, _id) in SENSOR_FEED_TYPES.items()
        },
        **{
            cv.Optional(marker, default=[]): cv.ensure_list(TSP_ENTRY_SCHEMA)
            for marker in TSP_FAMILY_DATA_IDS
        },
    }
)


def _validate_control_setpoint_configured(config: ConfigType) -> ConfigType:
    """§5.2.1: CONTROL_SETPOINT (id=1) is one of the spec's two mandatory ids -- matching hub.h's
    unconditional add_entry_(RequestKind::CONTROL_SETPOINT) in build_schedule_(). Checked against the
    full merged config (every number.opentherm42 block from every included package for this specific
    hub), not just this one schema instance -- packages merge same-named list keys (like the
    top-level number:) by concatenation, not by dict key, so a device config that splits its
    number.opentherm42 entities across several files ends up with one separate list item per file.
    A plain cv.Required(marker) in CONFIG_SCHEMA would then wrongly demand the marker in every one
    of those files instead of just one.
    """
    full_config = fv.full_config.get()
    hub_id = config[CONF_OPENTHERM42_ID]
    for entry in full_config.get(CONF_NUMBER, []):
        if entry.get(CONF_PLATFORM) != "opentherm42":
            continue
        if entry.get(CONF_OPENTHERM42_ID) != hub_id:
            continue
        if CONF_CONTROL_AND_STATUS_INFORMATION_CONTROL_SETPOINT in entry:
            return config
    raise cv.Invalid(
        f"'{CONF_CONTROL_AND_STATUS_INFORMATION_CONTROL_SETPOINT}' is required somewhere in the "
        "configuration for this opentherm42 hub -- §5.2.1: control setpoint is one of the "
        "protocol's two mandatory ids."
    )


FINAL_VALIDATE_SCHEMA = _validate_control_setpoint_configured


async def to_code(config: dict) -> None:
    hub = await cg.get_variable(config[CONF_OPENTHERM42_ID])
    for marker, (_schema, traits, data_id) in TYPES.items():
        if (marker_config := config.get(marker)) is not None:
            var = await number.new_number(marker_config, hub, data_id, **traits)
            await cg.register_component(var, marker_config)
            cg.add(var.set_initial_value(marker_config[CONF_INITIAL_VALUE]))
            cg.add(var.set_update_every(marker_config[CONF_UPDATE_EVERY]))
            cg.add(getattr(hub, f"set_{marker}_number")(var))

    for marker, (_schema, traits, data_id) in SENSOR_FEED_TYPES.items():
        if (marker_config := config.get(marker)) is not None:
            var = await number.new_number(marker_config, hub, data_id, **traits)
            await cg.register_component(var, marker_config)
            if (update_every := marker_config.get(CONF_UPDATE_EVERY)) is not None:
                cg.add(var.set_update_every(update_every))
            cg.add(getattr(hub, f"set_{marker}_number")(var))

    slot_index = 0
    for marker, data_id in TSP_FAMILY_DATA_IDS.items():
        for entry in config[marker]:
            var = await number.new_number(
                entry, hub, slot_index, min_value=0, max_value=255, step=1
            )
            await cg.register_component(var, entry)
            cg.add(hub.add_tsp_slot(data_id, entry[CONF_INDEX], var))
            slot_index += 1
