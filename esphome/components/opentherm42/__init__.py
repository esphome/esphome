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

# §5.2's mandatory heartbeat (id=0): unconditionally required, since STATUS is unconditionally
# scheduled regardless of which, if any, of its switch/binary_sensor bits are configured -- see
# hub.h's ReservedEntry. Named after the exact marker prefix its read-side binary_sensors use
# (control_and_status_information_boiler_status_*) -- its write-side switches use a different
# prefix (master_status_*) for the same id, so there's no single natural shared name; the read
# side was chosen since "update interval" is conceptually about how often the boiler's own
# reported data gets refreshed.
CONF_CONTROL_AND_STATUS_INFORMATION_BOILER_STATUS_UPDATE_INTERVAL = (
    "control_and_status_information_boiler_status_update_interval"
)

# Every id below drives more than one entity from a single conversation, so its update_interval
# lives here at the hub level rather than on any one entity -- see hub.h's ScheduledEntry and the
# PR that introduced these for the full catalog/naming rationale. Each is cv.Optional (no default:
# presence alone signals the group is active) and required only if any of that group's entities is
# configured, enforced per-platform by validate_requires_hub_option() below.
CONF_CONTROL_AND_STATUS_INFORMATION_STATUS_VENTILATION_HEAT_RECOVERY_UPDATE_INTERVAL = (
    "control_and_status_information_status_ventilation_heat_recovery_update_interval"
)
CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_UPDATE_INTERVAL = (
    "control_and_status_information_application_specific_fault_flags_update_interval"
)
CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_VENTILATION_HEAT_RECOVERY_UPDATE_INTERVAL = "control_and_status_information_application_specific_fault_flags_ventilation_heat_recovery_update_interval"
CONF_CONTROL_AND_STATUS_INFORMATION_SOLAR_STORAGE_MODE_AND_STATUS_UPDATE_INTERVAL = (
    "control_and_status_information_solar_storage_mode_and_status_update_interval"
)
CONF_CONFIGURATION_INFORMATION_CONFIGURATION_VENTILATION_HEAT_RECOVERY_UPDATE_INTERVAL = "configuration_information_configuration_ventilation_heat_recovery_update_interval"
CONF_CONFIGURATION_INFORMATION_SOLAR_STORAGE_CONFIGURATION_UPDATE_INTERVAL = (
    "configuration_information_solar_storage_configuration_update_interval"
)
CONF_CONFIGURATION_INFORMATION_BOILER_PRODUCT_VERSION_NUMBER_AND_TYPE_UPDATE_INTERVAL = "configuration_information_boiler_product_version_number_and_type_update_interval"
CONF_CONFIGURATION_INFORMATION_VENTILATION_HEAT_RECOVERY_PRODUCT_VERSION_NUMBER_AND_TYPE_UPDATE_INTERVAL = "configuration_information_ventilation_heat_recovery_product_version_number_and_type_update_interval"
CONF_CONFIGURATION_INFORMATION_SOLAR_STORAGE_PRODUCT_VERSION_NUMBER_AND_TYPE_UPDATE_INTERVAL = "configuration_information_solar_storage_product_version_number_and_type_update_interval"
CONF_SENSOR_AND_INFORMATIONAL_DATA_BOILER_FAN_SPEED_UPDATE_INTERVAL = (
    "sensor_and_informational_data_boiler_fan_speed_update_interval"
)
CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_FLAGS_UPDATE_INTERVAL = (
    "pre_defined_remote_boiler_parameters_flags_update_interval"
)
CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_VENTILATION_HEAT_RECOVERY_FLAGS_UPDATE_INTERVAL = "pre_defined_remote_boiler_parameters_ventilation_heat_recovery_flags_update_interval"
CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_DHWSETP_UPDATE_INTERVAL = (
    "pre_defined_remote_boiler_parameters_dhwsetp_update_interval"
)
CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_MAX_CHSETP_UPDATE_INTERVAL = (
    "pre_defined_remote_boiler_parameters_max_chsetp_update_interval"
)
CONF_CONTROL_OF_SPECIAL_APPLICATIONS_MAX_CAPACITY_MIN_MOD_LEVEL_UPDATE_INTERVAL = (
    "control_of_special_applications_max_capacity_min_mod_level_update_interval"
)
CONF_CONTROL_OF_SPECIAL_APPLICATIONS_REMOTE_OVERRIDE_OPERATING_MODE_UPDATE_INTERVAL = (
    "control_of_special_applications_remote_override_operating_mode_update_interval"
)
CONF_CONTROL_OF_SPECIAL_APPLICATIONS_REMOTE_OVERRIDE_ROOM_SETPOINT_FUNCTION_UPDATE_INTERVAL = "control_of_special_applications_remote_override_room_setpoint_function_update_interval"
# §5.3.6 Class 6, IDs 11/89/106: all three TSP families round-robin through one shared
# conversation (see hub.h's tsp_slots_) -- this governs how fast that rotation advances as a
# whole, not any individual slot's own refresh rate. On-demand writes always jump the queue ahead
# of it regardless.
CONF_TRANSPARENT_BOILER_PARAMETERS_UPDATE_INTERVAL = (
    "transparent_boiler_parameters_update_interval"
)
# §5.3.7 Class 7, IDs 13/91/108: same shared-rotation reasoning as TSP above, for the (purely
# read-only) fault-history-buffer entries.
CONF_FAULT_HISTORY_DATA_FAULT_BUFFER_UPDATE_INTERVAL = (
    "fault_history_data_fault_buffer_update_interval"
)

# One hub option per 1:N group, used both to build CONFIG_SCHEMA below and by to_code() to only
# emit the matching setter call when the option is actually present.
GROUP_UPDATE_INTERVAL_OPTIONS = (
    CONF_CONTROL_AND_STATUS_INFORMATION_STATUS_VENTILATION_HEAT_RECOVERY_UPDATE_INTERVAL,
    CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_UPDATE_INTERVAL,
    CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_VENTILATION_HEAT_RECOVERY_UPDATE_INTERVAL,
    CONF_CONTROL_AND_STATUS_INFORMATION_SOLAR_STORAGE_MODE_AND_STATUS_UPDATE_INTERVAL,
    CONF_CONFIGURATION_INFORMATION_CONFIGURATION_VENTILATION_HEAT_RECOVERY_UPDATE_INTERVAL,
    CONF_CONFIGURATION_INFORMATION_SOLAR_STORAGE_CONFIGURATION_UPDATE_INTERVAL,
    CONF_CONFIGURATION_INFORMATION_BOILER_PRODUCT_VERSION_NUMBER_AND_TYPE_UPDATE_INTERVAL,
    CONF_CONFIGURATION_INFORMATION_VENTILATION_HEAT_RECOVERY_PRODUCT_VERSION_NUMBER_AND_TYPE_UPDATE_INTERVAL,
    CONF_CONFIGURATION_INFORMATION_SOLAR_STORAGE_PRODUCT_VERSION_NUMBER_AND_TYPE_UPDATE_INTERVAL,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_BOILER_FAN_SPEED_UPDATE_INTERVAL,
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_FLAGS_UPDATE_INTERVAL,
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_VENTILATION_HEAT_RECOVERY_FLAGS_UPDATE_INTERVAL,
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_DHWSETP_UPDATE_INTERVAL,
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_MAX_CHSETP_UPDATE_INTERVAL,
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_MAX_CAPACITY_MIN_MOD_LEVEL_UPDATE_INTERVAL,
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_REMOTE_OVERRIDE_OPERATING_MODE_UPDATE_INTERVAL,
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_REMOTE_OVERRIDE_ROOM_SETPOINT_FUNCTION_UPDATE_INTERVAL,
    CONF_TRANSPARENT_BOILER_PARAMETERS_UPDATE_INTERVAL,
    CONF_FAULT_HISTORY_DATA_FAULT_BUFFER_UPDATE_INTERVAL,
)

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


def validate_requires_hub_option(markers: list[str], hub_option: str, group_label: str):
    """FINAL_VALIDATE_SCHEMA factory for a group of entities that share one conversation (see
    hub.h's ScheduledEntry): fails config validation, rather than silently using a placeholder
    interval, if any of `markers` is configured but the opentherm42 hub's `hub_option` isn't set.
    Only needs to check the markers that live in the calling platform file -- if a sibling platform
    (e.g. binary_sensor for a group also configured via switch) has a triggering marker instead,
    that platform's own validate_requires_hub_option() call independently enforces the same rule.

    Checks truthiness, not mere key presence: TSP/FHB's family markers are `cv.Optional(...,
    default=[])`, so the key is always present in the validated config even when the user configured
    no slots at all -- an empty list must count as "not configured", same as a genuinely absent key.
    """

    def _validate(config: ConfigType) -> ConfigType:
        if not any(config.get(marker) for marker in markers):
            return config
        full_config = fv.full_config.get()
        hub_path = full_config.get_path_for_id(config[CONF_OPENTHERM42_ID])[:-1]
        hub_config = full_config.get_config_for_path(hub_path)
        if hub_option not in hub_config:
            raise cv.Invalid(
                f"'{hub_option}' on the opentherm42 hub is required when {group_label} is configured",
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
            # some ids, self-correcting within a second or two with no apparent cause. Default 0
            # disables this grace period entirely (an entity goes to Unknown on the very first
            # DATA_INVALID, as before this option existed) -- see hub.h's set_max_data_invalid().
            cv.Optional(
                CONF_MAX_DATA_INVALID, default="0s"
            ): cv.positive_time_period_milliseconds,
            # §5.2's mandatory heartbeat -- see the constant's own comment above. Range-capped so a
            # user can't accidentally violate §4.3.1's 1.15 s MCI ceiling.
            cv.Required(
                CONF_CONTROL_AND_STATUS_INFORMATION_BOILER_STATUS_UPDATE_INTERVAL
            ): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(max=cv.TimePeriod(milliseconds=1150)),
            ),
            **{
                cv.Optional(option): cv.positive_time_period_milliseconds
                for option in GROUP_UPDATE_INTERVAL_OPTIONS
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

    cg.add(
        getattr(
            var,
            f"set_{CONF_CONTROL_AND_STATUS_INFORMATION_BOILER_STATUS_UPDATE_INTERVAL}",
        )(config[CONF_CONTROL_AND_STATUS_INFORMATION_BOILER_STATUS_UPDATE_INTERVAL])
    )
    for option in GROUP_UPDATE_INTERVAL_OPTIONS:
        if (interval := config.get(option)) is not None:
            cg.add(getattr(var, f"set_{option}")(interval))

    if CORE.is_esp32:
        # §4.3/§3.3.2 bit-timing (datalink.h) needs a hardware timer for microsecond-accurate sampling.
        include_builtin_idf_component("esp_driver_gptimer")
