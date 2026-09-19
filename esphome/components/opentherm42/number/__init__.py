import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_INDEX,
    CONF_INITIAL_VALUE,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_CONFIG,
)

from .. import OpenTherm42Hub, opentherm42_ns
from ..const import (
    CONF_CONTROL_AND_STATUS_INFORMATION_CONTROL_SETPOINT,
    CONF_CONTROL_AND_STATUS_INFORMATION_CONTROL_SETPOINT_2_TSETCH2,
    CONF_CONTROL_AND_STATUS_INFORMATION_CONTROL_SETPOINT_VENTILATION_HEAT_RECOVERY,
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_COOLING_CONTROL_SIGNAL,
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_MAXIMUM_RELATIVE_MODULATION_LEVEL_SETTING,
    CONF_OPENTHERM42_ID,
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_DHW_SETPOINT,
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_MAX_CH_WATER_SETPOINT,
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_NOMINAL_VENTILATION_VALUE,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_CO2_LEVEL,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_OUTSIDE_TEMPERATURE,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_RELATIVE_HUMIDITY,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_RELATIVE_HUMIDITY_EXHAUST_AIR,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_ROOM_SETPOINT,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_ROOM_SETPOINT_CH2,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_ROOM_TEMPERATURE,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_TRCH2,
    CONF_TRANSPARENT_BOILER_PARAMETERS,
    CONF_TRANSPARENT_BOILER_PARAMETERS_SOLAR_STORAGE,
    CONF_TRANSPARENT_BOILER_PARAMETERS_VENTILATION_HEAT_RECOVERY,
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
        _number_schema("°C", 0, 100, device_class=DEVICE_CLASS_TEMPERATURE),
        {"min_value": 0, "max_value": 100, "step": 0.1},
        1,
    ),
    # §5.3.1 Class 1, ID 8: Control Setpoint 2 (TsetCH2), i.e. setpoint for the 2nd CH circuit
    # (degrees C, 0..100).
    CONF_CONTROL_AND_STATUS_INFORMATION_CONTROL_SETPOINT_2_TSETCH2: (
        _number_schema("°C", 0, 100, device_class=DEVICE_CLASS_TEMPERATURE),
        {"min_value": 0, "max_value": 100, "step": 0.1},
        8,
    ),
    # §5.3.1 Class 1, ID 71 LB: Control Setpoint ventilation/heat-recovery. Relative ventilation
    # position (0-100%): 0% is the minimum set ventilation, 100% is the maximum set ventilation.
    CONF_CONTROL_AND_STATUS_INFORMATION_CONTROL_SETPOINT_VENTILATION_HEAT_RECOVERY: (
        _number_schema("%", 0, 100),
        {"min_value": 0, "max_value": 100, "step": 1},
        71,
    ),
    # §5.3.4 Class 4, ID 16: Room Setpoint -- current room temperature setpoint (degrees C, -40..127).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_ROOM_SETPOINT: (
        _number_schema("°C", -40, 127, device_class=DEVICE_CLASS_TEMPERATURE),
        {"min_value": -40, "max_value": 127, "step": 0.1},
        16,
    ),
    # §5.3.4 Class 4, ID 23: Room Setpoint CH2 -- current room setpoint for the 2nd CH circuit
    # (degrees C, -40..127).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_ROOM_SETPOINT_CH2: (
        _number_schema("°C", -40, 127, device_class=DEVICE_CLASS_TEMPERATURE),
        {"min_value": -40, "max_value": 127, "step": 0.1},
        23,
    ),
    # §5.3.4 Class 4, ID 24: Room temperature -- this master's own sensed room temperature
    # (degrees C, -40..127), pushed to the boiler. A sensor-value feed rather than a demand, so
    # it's CONFIG rather than primary.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_ROOM_TEMPERATURE: (
        _number_schema(
            "°C",
            -40,
            127,
            device_class=DEVICE_CLASS_TEMPERATURE,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        {"min_value": -40, "max_value": 127, "step": 0.1},
        24,
    ),
    # §5.3.4 Class 4, ID 37: TrCH2 -- room temperature for the 2nd CH circuit (degrees C, -40..127),
    # same sensor-value-feed nature as ID 24 above.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_TRCH2: (
        _number_schema(
            "°C",
            -40,
            127,
            device_class=DEVICE_CLASS_TEMPERATURE,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        {"min_value": -40, "max_value": 127, "step": 0.1},
        37,
    ),
    # §5.3.5 Class 5, ID 56: DHW Setpoint -- domestic hot water temperature setpoint (degrees C,
    # 0..127). A single number entity serves both directions -- see hub.h's RequestKind comment for
    # why only a successful read ever updates what's displayed.
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_DHW_SETPOINT: (
        _number_schema("°C", 0, 127, device_class=DEVICE_CLASS_TEMPERATURE),
        {"min_value": 0, "max_value": 127, "step": 0.1},
        56,
    ),
    # §5.3.5 Class 5, ID 57: max CH water Setpoint -- maximum allowable CH water Setpoint (degrees C,
    # 0..127). Same read/write sharing as ID 56 above. An installation-time ceiling on ID 1's Control
    # Setpoint, not something adjusted day-to-day, so CONFIG.
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_MAX_CH_WATER_SETPOINT: (
        _number_schema(
            "°C",
            0,
            127,
            device_class=DEVICE_CLASS_TEMPERATURE,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        {"min_value": 0, "max_value": 127, "step": 0.1},
        57,
    ),
    # §5.3.5 Class 5, ID 87 HB: Nominal ventilation value -- nominal relative value for ventilation
    # (0-100%), i.e. the value for the mid position in case of a 3-speed ventilation system. Same
    # read/write sharing as ID 56 above. A fixed system parameter rather than a live demand, so
    # CONFIG.
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_NOMINAL_VENTILATION_VALUE: (
        _number_schema("%", 0, 100, entity_category=ENTITY_CATEGORY_CONFIG),
        {"min_value": 0, "max_value": 100, "step": 1},
        87,
    ),
    # §5.3.8.1 Class 8, ID 7: Cooling control signal -- signal for the cooling plant (0..100%).
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_COOLING_CONTROL_SIGNAL: (
        _number_schema("%", 0, 100),
        {"min_value": 0, "max_value": 100, "step": 1},
        7,
    ),
    # §5.3.8.2 Class 8, ID 14: Maximum relative modulation level setting, for sequencer and
    # off-low&pump control applications (0..100%). An advanced, installation-specific tuning
    # parameter, so CONFIG.
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_MAXIMUM_RELATIVE_MODULATION_LEVEL_SETTING: (
        _number_schema("%", 0, 100, entity_category=ENTITY_CATEGORY_CONFIG),
        {"min_value": 0, "max_value": 100, "step": 1},
        14,
    ),
}

# §5.3.4 Class 4, IDs 27/38/78/79: this master's own external sensor readings, pushed to the boiler.
# Unlike TYPES above, these take no initial_value -- there's nothing for this component to invent a
# default for (see OpenTherm42SensorFeedNumber's class comment) -- and the constructor also needs the
# OpenTherm data-id, which the hub uses to route control()'s value to the right internal field (see
# hub.h's set_sensor_feed_write_value()). Keyed by marker -> (schema, number.new_number traits, id).
SENSOR_FEED_TYPES: dict[str, tuple[cv.Schema, dict, int]] = {
    # §5.3.4 Class 4, ID 27: Outside temperature (degrees C, -40..127). A sensor-value feed, so CONFIG.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_OUTSIDE_TEMPERATURE: (
        number.number_schema(
            OpenTherm42SensorFeedNumber,
            unit_of_measurement="°C",
            device_class=DEVICE_CLASS_TEMPERATURE,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        {"min_value": -40, "max_value": 127, "step": 0.1},
        27,
    ),
    # §5.3.4 Class 4, ID 38: Relative Humidity (0..100%). Same nature as ID 27 above.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_RELATIVE_HUMIDITY: (
        number.number_schema(
            OpenTherm42SensorFeedNumber,
            unit_of_measurement="%",
            device_class=DEVICE_CLASS_HUMIDITY,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        {"min_value": 0, "max_value": 100, "step": 1},
        38,
    ),
    # §5.3.4 Class 4, ID 78 LB: Relative humidity exhaust air (0..100%). Same nature as ID 27 above.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_RELATIVE_HUMIDITY_EXHAUST_AIR: (
        number.number_schema(
            OpenTherm42SensorFeedNumber,
            unit_of_measurement="%",
            device_class=DEVICE_CLASS_HUMIDITY,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        {"min_value": 0, "max_value": 100, "step": 1},
        78,
    ),
    # §5.3.4 Class 4, ID 79: CO2 level exhaust air (0..2000 ppm). Same nature as ID 27 above.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_CO2_LEVEL: (
        number.number_schema(
            OpenTherm42SensorFeedNumber,
            unit_of_measurement="ppm",
            device_class=DEVICE_CLASS_CARBON_DIOXIDE,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        {"min_value": 0, "max_value": 2000, "step": 1},
        79,
    ),
}

# §5.3.6 Class 6, IDs 11/89/106: one list of user-named TSP slots per family, keyed by which data-id
# reads/writes that family's transparent-boiler-parameters.
TSP_FAMILY_DATA_IDS: dict[str, int] = {
    CONF_TRANSPARENT_BOILER_PARAMETERS: 11,
    CONF_TRANSPARENT_BOILER_PARAMETERS_VENTILATION_HEAT_RECOVERY: 89,
    CONF_TRANSPARENT_BOILER_PARAMETERS_SOLAR_STORAGE: 106,
}

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


async def to_code(config: dict) -> None:
    hub = await cg.get_variable(config[CONF_OPENTHERM42_ID])
    for marker, (_schema, traits, data_id) in TYPES.items():
        if (marker_config := config.get(marker)) is not None:
            var = await number.new_number(marker_config, hub, data_id, **traits)
            await cg.register_component(var, marker_config)
            cg.add(var.set_initial_value(marker_config[CONF_INITIAL_VALUE]))
            cg.add(getattr(hub, f"set_{marker}_number")(var))

    for marker, (_schema, traits, data_id) in SENSOR_FEED_TYPES.items():
        if (marker_config := config.get(marker)) is not None:
            var = await number.new_number(marker_config, hub, data_id, **traits)
            await cg.register_component(var, marker_config)
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
