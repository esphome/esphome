import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_INITIAL_VALUE, DEVICE_CLASS_TEMPERATURE

from .. import OpenTherm42Hub, opentherm42_ns
from ..const import (
    CONF_CONTROL_AND_STATUS_INFORMATION_CONTROL_SETPOINT,
    CONF_OPENTHERM42_ID,
    CONF_UPDATE_EVERY,
)

OpenTherm42Number = opentherm42_ns.class_(
    "OpenTherm42Number", number.Number, cg.Component
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

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_OPENTHERM42_ID): cv.use_id(OpenTherm42Hub),
        **{
            cv.Optional(marker): schema.extend(cv.COMPONENT_SCHEMA)
            for marker, (schema, _traits, _id) in TYPES.items()
        },
    }
)


async def to_code(config: dict) -> None:
    hub = await cg.get_variable(config[CONF_OPENTHERM42_ID])
    for marker, (_schema, traits, data_id) in TYPES.items():
        if (marker_config := config.get(marker)) is not None:
            var = await number.new_number(marker_config, hub, data_id, **traits)
            await cg.register_component(var, marker_config)
            cg.add(var.set_initial_value(marker_config[CONF_INITIAL_VALUE]))
            cg.add(var.set_update_every(marker_config[CONF_UPDATE_EVERY]))
            cg.add(getattr(hub, f"set_{marker}_number")(var))
