from esphome import pins
import esphome.codegen as cg
from esphome.components import time as time_
from esphome.components.esp32 import include_builtin_idf_component
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TIME_ID, PLATFORM_ESP32, PLATFORM_ESP8266
from esphome.core import CORE
import esphome.final_validate as fv
from esphome.types import ConfigType

from .const import CONF_OPENTHERM42_ID

CODEOWNERS = ["@fornellas"]
MULTI_CONF = True
# hub.h unconditionally declares fields of each of these types (e.g. every possible boiler sensor,
# even ones the user hasn't configured), so their headers must always be compiled in -- regardless
# of whether the user's YAML happens to configure any entities under these domains.
AUTO_LOAD = [
    "binary_sensor",
    "number",
    "select",
    "sensor",
    "switch",
    "text_sensor",
    "time",
]

CONF_IN_PIN = "in_pin"
CONF_OUT_PIN = "out_pin"
CONF_MAX_DATA_INVALID = "max_data_invalid"

# §5.2's mandatory heartbeat (id=0): unconditionally scheduled regardless of which, if any, of its
# switch/binary_sensor bits are configured -- see hub.h's Entry. Named after the exact marker prefix
# its read-side binary_sensors use (control_and_status_information_boiler_status_*) -- its
# write-side switches use a different prefix (master_status_*) for the same id, so there's no
# single natural shared name; the read side was chosen since "update_every" is conceptually about
# how often the boiler's own reported data gets refreshed.
CONF_CONTROL_AND_STATUS_INFORMATION_BOILER_STATUS_UPDATE_EVERY = (
    "control_and_status_information_boiler_status_update_every"
)
# §5.3.2 Class 2, ID 3: boiler configuration flags + boiler MemberID code -- a group spanning more
# than one entity (8 text_sensors + 1 sensor), like every other id in UPDATE_EVERY_OPTIONS below.
CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_UPDATE_EVERY = (
    "configuration_information_boiler_configuration_update_every"
)
# §5.3.2 Class 2, IDs 2/124/126: this master's own identity, announced to the boiler -- no entity
# of its own (Smart Power capability flag / this component's own OpenTherm version / this
# component's own product identity, none of which are ever configured per-entity), so each gets
# its own hub-level option, same reasoning as STATUS above.
CONF_CONFIGURATION_INFORMATION_MASTER_CONFIGURATION_UPDATE_EVERY = (
    "configuration_information_master_configuration_update_every"
)
CONF_CONFIGURATION_INFORMATION_MASTER_OPENTHERM_VERSION_UPDATE_EVERY = (
    "configuration_information_master_opentherm_version_update_every"
)
CONF_CONFIGURATION_INFORMATION_MASTER_PRODUCT_VERSION_UPDATE_EVERY = (
    "configuration_information_master_product_version_update_every"
)

# Every id below drives more than one entity from a single conversation, so its update_every lives
# here at the hub level rather than on any one entity -- see hub.h's Entry and the PR that
# introduced these for the full catalog/naming rationale.
CONF_CONTROL_AND_STATUS_INFORMATION_STATUS_VENTILATION_HEAT_RECOVERY_UPDATE_EVERY = (
    "control_and_status_information_status_ventilation_heat_recovery_update_every"
)
CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_UPDATE_EVERY = (
    "control_and_status_information_application_specific_fault_flags_update_every"
)
CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_VENTILATION_HEAT_RECOVERY_UPDATE_EVERY = "control_and_status_information_application_specific_fault_flags_ventilation_heat_recovery_update_every"
CONF_CONTROL_AND_STATUS_INFORMATION_SOLAR_STORAGE_MODE_AND_STATUS_UPDATE_EVERY = (
    "control_and_status_information_solar_storage_mode_and_status_update_every"
)
CONF_CONFIGURATION_INFORMATION_CONFIGURATION_VENTILATION_HEAT_RECOVERY_UPDATE_EVERY = (
    "configuration_information_configuration_ventilation_heat_recovery_update_every"
)
CONF_CONFIGURATION_INFORMATION_SOLAR_STORAGE_CONFIGURATION_UPDATE_EVERY = (
    "configuration_information_solar_storage_configuration_update_every"
)
CONF_CONFIGURATION_INFORMATION_BOILER_PRODUCT_VERSION_NUMBER_AND_TYPE_UPDATE_EVERY = (
    "configuration_information_boiler_product_version_number_and_type_update_every"
)
CONF_CONFIGURATION_INFORMATION_VENTILATION_HEAT_RECOVERY_PRODUCT_VERSION_NUMBER_AND_TYPE_UPDATE_EVERY = "configuration_information_ventilation_heat_recovery_product_version_number_and_type_update_every"
CONF_CONFIGURATION_INFORMATION_SOLAR_STORAGE_PRODUCT_VERSION_NUMBER_AND_TYPE_UPDATE_EVERY = "configuration_information_solar_storage_product_version_number_and_type_update_every"
CONF_SENSOR_AND_INFORMATIONAL_DATA_BOILER_FAN_SPEED_UPDATE_EVERY = (
    "sensor_and_informational_data_boiler_fan_speed_update_every"
)
CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_FLAGS_UPDATE_EVERY = (
    "pre_defined_remote_boiler_parameters_flags_update_every"
)
CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_VENTILATION_HEAT_RECOVERY_FLAGS_UPDATE_EVERY = "pre_defined_remote_boiler_parameters_ventilation_heat_recovery_flags_update_every"
CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_DHWSETP_UPDATE_EVERY = (
    "pre_defined_remote_boiler_parameters_dhwsetp_update_every"
)
CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_MAX_CHSETP_UPDATE_EVERY = (
    "pre_defined_remote_boiler_parameters_max_chsetp_update_every"
)
CONF_CONTROL_OF_SPECIAL_APPLICATIONS_MAX_CAPACITY_MIN_MOD_LEVEL_UPDATE_EVERY = (
    "control_of_special_applications_max_capacity_min_mod_level_update_every"
)
CONF_CONTROL_OF_SPECIAL_APPLICATIONS_REMOTE_OVERRIDE_OPERATING_MODE_UPDATE_EVERY = (
    "control_of_special_applications_remote_override_operating_mode_update_every"
)
CONF_CONTROL_OF_SPECIAL_APPLICATIONS_REMOTE_OVERRIDE_ROOM_SETPOINT_FUNCTION_UPDATE_EVERY = "control_of_special_applications_remote_override_room_setpoint_function_update_every"
# §5.3.6 Class 6, IDs 11/89/106: all three TSP families round-robin through one shared
# conversation (see hub.h's tsp_slots_) -- this governs how fast that rotation advances as a
# whole, not any individual slot's own refresh rate. On-demand writes always jump the queue ahead
# of it regardless.
CONF_TRANSPARENT_BOILER_PARAMETERS_UPDATE_EVERY = (
    "transparent_boiler_parameters_update_every"
)
# §5.3.7 Class 7, IDs 13/91/108: same shared-rotation reasoning as TSP above, for the (purely
# read-only) fault-history-buffer entries.
CONF_FAULT_HISTORY_DATA_FAULT_BUFFER_UPDATE_EVERY = (
    "fault_history_data_fault_buffer_update_every"
)

# Every hub-level update_every option, with the default cadence (in passes) it takes when the user
# doesn't override it -- see hub.h's Entry for what a pass/cadence means. All cv.Optional: none of
# these need to be set at all unless you want a different cadence than the default, whether or not
# the group's entities are even configured (see CONFIG_SCHEMA/to_code() below -- the hub-side setter
# is harmless to call even for a group with no configured entities, since build_schedule_() only
# ever applies a staged cadence to an Entry that actually exists). Defaults were chosen by grouping
# every schedulable id into 8 priority tiers (1 = fastest/mandatory, 8 = slowest/lifetime counters)
# and picking update_every = tier number -- the smallest consecutive integer sequence available,
# since a tier's real refresh interval is tier_number * (time for one full pass), so any gap would
# only slow a tier down for no benefit. See the PR that introduced this scheme for the full
# tier-by-tier catalog and the bus-timing math (§4.3.1) the 8-tier spread was checked against.
UPDATE_EVERY_OPTIONS: dict[str, int] = {
    # Tier 1: the spec's mandatory heartbeat.
    CONF_CONTROL_AND_STATUS_INFORMATION_BOILER_STATUS_UPDATE_EVERY: 1,
    # Tier 6: static configuration/capability data, no entity of its own.
    CONF_CONFIGURATION_INFORMATION_MASTER_CONFIGURATION_UPDATE_EVERY: 6,
    CONF_CONFIGURATION_INFORMATION_MASTER_OPENTHERM_VERSION_UPDATE_EVERY: 6,
    CONF_CONFIGURATION_INFORMATION_MASTER_PRODUCT_VERSION_UPDATE_EVERY: 6,
    CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_UPDATE_EVERY: 6,
    CONF_CONFIGURATION_INFORMATION_CONFIGURATION_VENTILATION_HEAT_RECOVERY_UPDATE_EVERY: 6,
    CONF_CONFIGURATION_INFORMATION_SOLAR_STORAGE_CONFIGURATION_UPDATE_EVERY: 6,
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_FLAGS_UPDATE_EVERY: 6,
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_VENTILATION_HEAT_RECOVERY_FLAGS_UPDATE_EVERY: 6,
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_DHWSETP_UPDATE_EVERY: 6,
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_MAX_CHSETP_UPDATE_EVERY: 6,
    # Tier 7: identification strings/versions and TSP/FHB round-robins.
    CONF_CONFIGURATION_INFORMATION_BOILER_PRODUCT_VERSION_NUMBER_AND_TYPE_UPDATE_EVERY: 7,
    CONF_CONFIGURATION_INFORMATION_VENTILATION_HEAT_RECOVERY_PRODUCT_VERSION_NUMBER_AND_TYPE_UPDATE_EVERY: 7,
    CONF_CONFIGURATION_INFORMATION_SOLAR_STORAGE_PRODUCT_VERSION_NUMBER_AND_TYPE_UPDATE_EVERY: 7,
    CONF_TRANSPARENT_BOILER_PARAMETERS_UPDATE_EVERY: 7,
    CONF_FAULT_HISTORY_DATA_FAULT_BUFFER_UPDATE_EVERY: 7,
    # Tier 4: status/fault-flag groups (event-driven, not continuously drifting).
    CONF_CONTROL_AND_STATUS_INFORMATION_STATUS_VENTILATION_HEAT_RECOVERY_UPDATE_EVERY: 4,
    CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_UPDATE_EVERY: 4,
    CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_VENTILATION_HEAT_RECOVERY_UPDATE_EVERY: 4,
    CONF_CONTROL_AND_STATUS_INFORMATION_SOLAR_STORAGE_MODE_AND_STATUS_UPDATE_EVERY: 4,
    # Tier 3: live process sensors that change during active operation.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_BOILER_FAN_SPEED_UPDATE_EVERY: 3,
}

# §5.3.2 Class 2: this master's own identity, written to the boiler once at startup (IDs 2 LB/126).
# Kept as static hub-level config rather than entities -- unlike the boiler's status/measurements,
# this doesn't change at runtime, so there's nothing for Home Assistant to show or control.
# ID 124 (this master's own OpenTherm version) isn't here: it's fixed to the version this component
# actually implements (hub.h's CONTROLLER_OPENTHERM_VERSION), not user-configurable -- see there.
CONF_CONTROLLER_MEMBER_ID_CODE = "controller_member_id_code"
CONF_CONTROLLER_PRODUCT_TYPE = "controller_product_type"
CONF_CONTROLLER_PRODUCT_VERSION = "controller_product_version"

opentherm42_ns = cg.esphome_ns.namespace("opentherm42")
OpenTherm42Hub = opentherm42_ns.class_("OpenTherm42Hub", cg.Component)


def validate_requires_time_id(marker: str):
    """FINAL_VALIDATE_SCHEMA factory for a platform entity that only makes sense when the opentherm42
    hub it references has time_id set (e.g. the time-sync status binary_sensor and button): fails
    config validation, rather than silently doing nothing at runtime, if marker is configured but the
    hub's time_id is not.
    """

    def _validate(config: ConfigType) -> ConfigType:
        if marker not in config:
            return config
        full_config = fv.full_config.get()
        hub_path = full_config.get_path_for_id(config[CONF_OPENTHERM42_ID])[:-1]
        hub_config = full_config.get_config_for_path(hub_path)
        if CONF_TIME_ID not in hub_config:
            raise cv.Invalid(
                f"'{marker}' requires the opentherm42 hub to have 'time_id' set",
                path=[marker],
            )
        return config

    return _validate


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(OpenTherm42Hub),
            cv.Required(CONF_IN_PIN): pins.internal_gpio_input_pin_schema,
            cv.Required(CONF_OUT_PIN): pins.internal_gpio_output_pin_schema,
            # §5.2.1 Note 2: a MemberID code of 0 signifies a customer non-specific device.
            cv.Optional(CONF_CONTROLLER_MEMBER_ID_CODE, default=0): cv.int_range(
                min=0, max=255
            ),
            cv.Optional(CONF_CONTROLLER_PRODUCT_TYPE, default=0): cv.int_range(
                min=0, max=255
            ),
            cv.Optional(CONF_CONTROLLER_PRODUCT_VERSION, default=0): cv.int_range(
                min=0, max=255
            ),
            # §5.3.4 Class 4, IDs 20/21/22: if set, this master keeps the boiler's Day-of-week/Time,
            # Date and Year synced to this clock. Left unset, those three ids are never sent.
            cv.Optional(CONF_TIME_ID): cv.use_id(time_.RealTimeClock),
            # §4.4.1: DATA_INVALID ("the data ID is recognised... but the data requested is not
            # available or invalid") has been observed on real hardware as a transient condition for
            # some ids, self-correcting within a few attempts with no apparent cause. Default 0
            # disables this grace period entirely (an entity goes to Unknown on the very first
            # DATA_INVALID, as before this option existed) -- see hub.h's set_max_data_invalid().
            # Counts consecutive DATA_INVALID responses for a given id, not wall-clock time: passes
            # aren't a fixed duration, so a time-based grace period wouldn't mean the same thing for
            # every id, unlike a plain retry count.
            cv.Optional(CONF_MAX_DATA_INVALID, default=0): cv.int_range(min=0),
            # No cap here (unlike the old ms-based interval this replaced): passes aren't wall-clock
            # time, so §4.3.1's 1.15 s MCI ceiling can't be expressed as a cap on these values -- see
            # build_next_request_() for how that's handled instead.
            **{
                cv.Optional(option, default=default): cv.int_range(min=1)
                for option, default in UPDATE_EVERY_OPTIONS.items()
            },
        }
    ).extend(cv.COMPONENT_SCHEMA),
    # datalink.cpp only implements a hardware-timer backend for ESP32 (gptimer) and ESP8266 (Arduino
    # Timer1) -- RP2040 and LibreTiny have no backend, so this matches the same restriction the
    # earlier opentherm component uses.
    cv.only_on([PLATFORM_ESP32, PLATFORM_ESP8266]),
)


async def to_code(config: dict) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    in_pin = await cg.gpio_pin_expression(config[CONF_IN_PIN])
    cg.add(var.set_in_pin(in_pin))
    out_pin = await cg.gpio_pin_expression(config[CONF_OUT_PIN])
    cg.add(var.set_out_pin(out_pin))

    cg.add(var.set_controller_member_id_code(config[CONF_CONTROLLER_MEMBER_ID_CODE]))
    cg.add(var.set_controller_product_type(config[CONF_CONTROLLER_PRODUCT_TYPE]))
    cg.add(var.set_controller_product_version(config[CONF_CONTROLLER_PRODUCT_VERSION]))

    if (time_id := config.get(CONF_TIME_ID)) is not None:
        time_var = await cg.get_variable(time_id)
        cg.add(var.set_time_id(time_var))

    cg.add(var.set_max_data_invalid(config[CONF_MAX_DATA_INVALID]))

    # Always present (every option defaults to its tier's cadence -- see UPDATE_EVERY_OPTIONS), so
    # every setter is called unconditionally; harmless for a group with no configured entities,
    # since build_schedule_() only ever applies a staged cadence to an Entry that actually exists.
    for option in UPDATE_EVERY_OPTIONS:
        cg.add(getattr(var, f"set_{option}")(config[option]))

    if CORE.is_esp32:
        # §4.3/§3.3.2 bit-timing (datalink.h) needs a hardware timer for microsecond-accurate sampling.
        include_builtin_idf_component("esp_driver_gptimer")
