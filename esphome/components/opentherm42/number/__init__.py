import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_INDEX, CONF_INITIAL_VALUE

from .. import OpenTherm42Hub, opentherm42_ns
from ..const import (
    CONF_CONTROL_AND_STATUS_INFORMATION_CONTROL_SETPOINT,
    CONF_CONTROL_AND_STATUS_INFORMATION_CONTROL_SETPOINT_2_TSETCH2,
    CONF_CONTROL_AND_STATUS_INFORMATION_CONTROL_SETPOINT_VENTILATION_HEAT_RECOVERY,
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_COOLING_CONTROL_SIGNAL,
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_MAXIMUM_RELATIVE_MODULATION_LEVEL_SETTING,
    CONF_OPENTHERM42_ID,
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_DHW_SETPOINT_SET,
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_MAX_CH_WATER_SETPOINT_SET,
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_NOMINAL_VENTILATION_VALUE_SET,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_CO2_LEVEL_SET,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_OUTSIDE_TEMPERATURE_SET,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_RELATIVE_HUMIDITY_EXHAUST_AIR_SET,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_RELATIVE_HUMIDITY_SET,
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
OpenTherm42TspNumber = opentherm42_ns.class_(
    "OpenTherm42TspNumber", number.Number, cg.Component
)


def _number_schema(
    unit_of_measurement: str, min_value: float, max_value: float, default: float
) -> cv.Schema:
    # Always restores its last value from flash on boot (see OpenTherm42Number) -- initial_value is
    # only the fallback for first boot or a corrupt/missing preference, not a way to opt out of restore.
    return number.number_schema(
        OpenTherm42Number, unit_of_measurement=unit_of_measurement
    ).extend(
        {
            cv.Optional(CONF_INITIAL_VALUE, default=default): cv.float_range(
                min=min_value, max=max_value
            ),
        }
    )


# §5.1: the master decides the actual min/max/step of the value it sends -- the spec only defines
# the wire range.
TYPES: dict[str, tuple[cv.Schema, dict]] = {
    # §5.3.1 Class 1, ID 1: Control Setpoint, i.e. CH water temperature setpoint (degrees C, 0..100).
    # The CHenable bit (see switch platform) has priority: the boiler must ignore this value while
    # CH is disabled.
    CONF_CONTROL_AND_STATUS_INFORMATION_CONTROL_SETPOINT: (
        _number_schema("°C", 0, 100, 40.0),
        {"min_value": 0, "max_value": 100, "step": 0.1},
    ),
    # §5.3.1 Class 1, ID 8: Control Setpoint 2 (TsetCH2), i.e. setpoint for the 2nd CH circuit
    # (degrees C, 0..100).
    CONF_CONTROL_AND_STATUS_INFORMATION_CONTROL_SETPOINT_2_TSETCH2: (
        _number_schema("°C", 0, 100, 40.0),
        {"min_value": 0, "max_value": 100, "step": 0.1},
    ),
    # §5.3.1 Class 1, ID 71 LB: Control Setpoint ventilation/heat-recovery. Relative ventilation
    # position (0-100%): 0% is the minimum set ventilation, 100% is the maximum set ventilation.
    CONF_CONTROL_AND_STATUS_INFORMATION_CONTROL_SETPOINT_VENTILATION_HEAT_RECOVERY: (
        _number_schema("%", 0, 100, 0),
        {"min_value": 0, "max_value": 100, "step": 1},
    ),
    # §5.3.4 Class 4, ID 16: Room Setpoint -- current room temperature setpoint (degrees C, -40..127).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_ROOM_SETPOINT: (
        _number_schema("°C", -40, 127, 20.0),
        {"min_value": -40, "max_value": 127, "step": 0.1},
    ),
    # §5.3.4 Class 4, ID 23: Room Setpoint CH2 -- current room setpoint for the 2nd CH circuit
    # (degrees C, -40..127).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_ROOM_SETPOINT_CH2: (
        _number_schema("°C", -40, 127, 20.0),
        {"min_value": -40, "max_value": 127, "step": 0.1},
    ),
    # §5.3.4 Class 4, ID 24: Room temperature -- this master's own sensed room temperature
    # (degrees C, -40..127), pushed to the boiler.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_ROOM_TEMPERATURE: (
        _number_schema("°C", -40, 127, 20.0),
        {"min_value": -40, "max_value": 127, "step": 0.1},
    ),
    # §5.3.4 Class 4, ID 37: TrCH2 -- room temperature for the 2nd CH circuit (degrees C, -40..127).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_TRCH2: (
        _number_schema("°C", -40, 127, 20.0),
        {"min_value": -40, "max_value": 127, "step": 0.1},
    ),
    # §5.3.4 Class 4, ID 27: Outside temperature (degrees C, -40..127), provided by this master. Only
    # written if configured -- see the sensor platform's plain outside_temperature for reading it from
    # the boiler instead.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_OUTSIDE_TEMPERATURE_SET: (
        _number_schema("°C", -40, 127, 20.0),
        {"min_value": -40, "max_value": 127, "step": 0.1},
    ),
    # §5.3.4 Class 4, ID 38: Relative Humidity (0..100%), provided by this master.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_RELATIVE_HUMIDITY_SET: (
        _number_schema("%", 0, 100, 50.0),
        {"min_value": 0, "max_value": 100, "step": 1},
    ),
    # §5.3.4 Class 4, ID 78 LB: Relative humidity exhaust air (0..100%), provided by this master.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_RELATIVE_HUMIDITY_EXHAUST_AIR_SET: (
        _number_schema("%", 0, 100, 50.0),
        {"min_value": 0, "max_value": 100, "step": 1},
    ),
    # §5.3.4 Class 4, ID 79: CO2 level exhaust air (0..2000 ppm), provided by this master.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_CO2_LEVEL_SET: (
        _number_schema("ppm", 0, 2000, 0),
        {"min_value": 0, "max_value": 2000, "step": 1},
    ),
    # §5.3.5 Class 5, ID 56: DHW Setpoint -- domestic hot water temperature setpoint (degrees C, 0..127).
    # Takes priority over the sensor platform's plain dhw_setpoint (reading the boiler's own value).
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_DHW_SETPOINT_SET: (
        _number_schema("°C", 0, 127, 60.0),
        {"min_value": 0, "max_value": 127, "step": 0.1},
    ),
    # §5.3.5 Class 5, ID 57: max CH water Setpoint -- maximum allowable CH water Setpoint (degrees C, 0..127).
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_MAX_CH_WATER_SETPOINT_SET: (
        _number_schema("°C", 0, 127, 80.0),
        {"min_value": 0, "max_value": 127, "step": 0.1},
    ),
    # §5.3.5 Class 5, ID 87 HB: Nominal ventilation value -- nominal relative value for ventilation
    # (0-100%), i.e. the value for the mid position in case of a 3-speed ventilation system.
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_NOMINAL_VENTILATION_VALUE_SET: (
        _number_schema("%", 0, 100, 50),
        {"min_value": 0, "max_value": 100, "step": 1},
    ),
    # §5.3.8.1 Class 8, ID 7: Cooling control signal -- signal for the cooling plant (0..100%).
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_COOLING_CONTROL_SIGNAL: (
        _number_schema("%", 0, 100, 0),
        {"min_value": 0, "max_value": 100, "step": 1},
    ),
    # §5.3.8.2 Class 8, ID 14: Maximum relative modulation level setting, for sequencer and
    # off-low&pump control applications (0..100%).
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_MAXIMUM_RELATIVE_MODULATION_LEVEL_SETTING: (
        _number_schema("%", 0, 100, 100),
        {"min_value": 0, "max_value": 100, "step": 1},
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
    number.number_schema(OpenTherm42TspNumber)
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
            for marker, (schema, _traits) in TYPES.items()
        },
        **{
            cv.Optional(marker, default=[]): cv.ensure_list(TSP_ENTRY_SCHEMA)
            for marker in TSP_FAMILY_DATA_IDS
        },
    }
)


async def to_code(config: dict) -> None:
    hub = await cg.get_variable(config[CONF_OPENTHERM42_ID])
    for marker, (_schema, traits) in TYPES.items():
        if (marker_config := config.get(marker)) is not None:
            var = await number.new_number(marker_config, **traits)
            await cg.register_component(var, marker_config)
            cg.add(var.set_initial_value(marker_config[CONF_INITIAL_VALUE]))
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
