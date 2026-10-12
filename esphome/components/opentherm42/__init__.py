from esphome import pins
import esphome.codegen as cg
from esphome.components import esp32, esp32_rmt
from esphome.components.const import CONF_OUT_PIN
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NUMBER, CONF_PLATFORM
import esphome.final_validate as fv
from esphome.types import ConfigType

from .const import (
    CONF_CONTROL_AND_STATUS_INFORMATION_CONTROL_SETPOINT,
    CONF_OPENTHERM42_ID,
)

CODEOWNERS = ["@fornellas"]
DOMAIN = "opentherm42"
MULTI_CONF = True
# hub.h unconditionally declares fields of each of these types (e.g. every possible boiler sensor,
# even ones the user hasn't configured), so their headers must always be compiled in -- regardless
# of whether the user's YAML happens to configure any entities under these domains.
AUTO_LOAD = [
    "binary_sensor",
    "number",
    "sensor",
    "switch",
]

CONF_IN_PIN = "in_pin"
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
}

opentherm42_ns = cg.esphome_ns.namespace("opentherm42")
OpenTherm42Hub = opentherm42_ns.class_("OpenTherm42Hub", cg.Component)


def _validate_rmt_available(config: ConfigType) -> ConfigType:
    """Rejects ESP32 variants without RMT hardware -- datalink_rmt.cpp is the only datalink backend."""
    if (variant := esp32.get_esp32_variant()) in esp32_rmt.VARIANTS_NO_RMT:
        raise cv.Invalid(
            f"opentherm42 needs the RMT peripheral, which {variant} doesn't have"
        )
    return config


def _validate_control_setpoint_configured(config: ConfigType) -> ConfigType:
    """§5.2.1: CONTROL_SETPOINT (id=1) is one of the spec's two mandatory ids -- matching hub.h's
    unconditional add_entry_(RequestKind::CONTROL_SETPOINT) in build_schedule_(). Checked here, on the
    hub, so a config with no number: opentherm42 block at all is rejected too. Looks at the full merged
    config (every number.opentherm42 block from every included package for this hub), not one schema
    instance -- packages merge same-named list keys (like the top-level number:) by concatenation, so
    a device config that splits its number.opentherm42 entities across several files ends up with one
    separate list item per file, and a plain cv.Required() would demand the marker in every one of them.
    """
    full_config = fv.full_config.get()
    for entry in full_config.get(CONF_NUMBER, []):
        if entry.get(CONF_PLATFORM) != DOMAIN:
            continue
        if entry.get(CONF_OPENTHERM42_ID) != config[CONF_ID]:
            continue
        if CONF_CONTROL_AND_STATUS_INFORMATION_CONTROL_SETPOINT in entry:
            return config
    raise cv.Invalid(
        f"A number: block with '{CONF_CONTROL_AND_STATUS_INFORMATION_CONTROL_SETPOINT}' is required for "
        "this opentherm42 hub -- §5.2.1: control setpoint is one of the protocol's two mandatory ids."
    )


FINAL_VALIDATE_SCHEMA = _validate_control_setpoint_configured

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(OpenTherm42Hub),
            cv.Required(CONF_IN_PIN): pins.internal_gpio_input_pin_schema,
            cv.Required(CONF_OUT_PIN): pins.internal_gpio_output_pin_schema,
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
    # datalink_rmt.cpp is the only datalink backend, and it needs the ESP32 RMT peripheral.
    cv.only_on_esp32,
    _validate_rmt_available,
)


async def to_code(config: dict) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    await cg.add_pin(var.set_in_pin, config[CONF_IN_PIN])
    await cg.add_pin(var.set_out_pin, config[CONF_OUT_PIN])

    cg.add(var.set_max_data_invalid(config[CONF_MAX_DATA_INVALID]))

    # Always present (every option defaults to its tier's cadence -- see UPDATE_EVERY_OPTIONS), so
    # every setter is called unconditionally; harmless for a group with no configured entities,
    # since build_schedule_() only ever applies a staged cadence to an Entry that actually exists.
    for option in UPDATE_EVERY_OPTIONS:
        cg.add(getattr(var, f"set_{option}")(config[option]))

    esp32.include_builtin_idf_component("esp_driver_rmt")
