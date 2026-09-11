import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_CAPACITY, CONF_ID, CONF_INITIAL_STATE
from esphome.core import ID
from esphome.helpers import fnv1_hash_object_id

CODEOWNERS = ["@clydebarrow"]
DEPENDENCIES = ["sensor"]
MULTI_CONF = True

battery_gauge_ns = cg.esphome_ns.namespace("battery_gauge")
BatteryGauge = battery_gauge_ns.class_("BatteryGauge", cg.Component)
LithiumChemistry = battery_gauge_ns.class_("LithiumChemistry")
LeadAcidChemistry = battery_gauge_ns.class_("LeadAcidChemistry")

CONF_BATTERY_GAUGE_ID = "battery_gauge_id"
CONF_CELL_COUNT = "cell_count"
CONF_CURRENT_SOURCE = "current_source"
CONF_EFFICIENCY = "efficiency"
CONF_MAX_CHARGE_VOLTAGE = "max_charge_voltage"
CONF_VOLTAGE_SOURCE = "voltage_source"

CONF_CHEMISTRY = "chemistry"
CONF_ACCEPTANCE_KNEE = "acceptance_knee"
CONF_PEUKERT_EXPONENT = "peukert_exponent"
CONF_CAPACITY_RATE = "capacity_rate"
CONF_TAIL_CURRENT = "tail_current"
CONF_FULL_CHARGE_DWELL = "full_charge_dwell"

CHEMISTRY_LIFEPO4 = "lifepo4"
CHEMISTRY_LEAD_ACID_FLOODED = "lead_acid_flooded"
CHEMISTRY_LEAD_ACID_AGM = "lead_acid_agm"
CHEMISTRY_CUSTOM = "custom"

CHEMISTRIES = (
    CHEMISTRY_LIFEPO4,
    CHEMISTRY_LEAD_ACID_FLOODED,
    CHEMISTRY_LEAD_ACID_AGM,
    CHEMISTRY_CUSTOM,
)

# Per-cell voltage at which each chemistry is considered fully charged, used to derive
# max_charge_voltage from cell_count. "custom" has no standard cell voltage to assume, so it is
# omitted here and must always be given as an explicit max_charge_voltage.
CELL_FULL_CHARGE_VOLTAGE = {
    CHEMISTRY_LIFEPO4: 3.65,
    CHEMISTRY_LEAD_ACID_FLOODED: 2.45,
    CHEMISTRY_LEAD_ACID_AGM: 2.40,
}

# Keys that only make sense for a lead-acid-style chemistry (flooded, AGM, or custom).
LEAD_ACID_KEYS = (
    CONF_ACCEPTANCE_KNEE,
    CONF_PEUKERT_EXPONENT,
    CONF_CAPACITY_RATE,
    CONF_TAIL_CURRENT,
    CONF_FULL_CHARGE_DWELL,
)

# Values these keys take when nothing that needs them is configured: an "accept anything,
# taper at the historical 2%, resync instantly" chemistry, identical to lifepo4's behaviour.
_NO_OP_DEFAULTS = {
    CONF_ACCEPTANCE_KNEE: 1.0,
    CONF_PEUKERT_EXPONENT: 1.0,
    CONF_CAPACITY_RATE: cv.TimePeriod(hours=20),
    CONF_TAIL_CURRENT: 0.02,
    CONF_FULL_CHARGE_DWELL: cv.TimePeriod(),
}

# Preset parameters for the built-in lead-acid variants. "custom" intentionally has no preset:
# every key falls back to _NO_OP_DEFAULTS unless the user overrides it.
LEAD_ACID_PRESETS = {
    CHEMISTRY_LEAD_ACID_FLOODED: {
        CONF_ACCEPTANCE_KNEE: 0.80,
        CONF_PEUKERT_EXPONENT: 1.25,
        CONF_CAPACITY_RATE: cv.TimePeriod(hours=20),
        CONF_TAIL_CURRENT: 0.04,
        CONF_FULL_CHARGE_DWELL: cv.TimePeriod(minutes=3),
    },
    CHEMISTRY_LEAD_ACID_AGM: {
        CONF_ACCEPTANCE_KNEE: 0.80,
        CONF_PEUKERT_EXPONENT: 1.15,
        CONF_CAPACITY_RATE: cv.TimePeriod(hours=20),
        CONF_TAIL_CURRENT: 0.04,
        CONF_FULL_CHARGE_DWELL: cv.TimePeriod(minutes=3),
    },
}


def _validate_chemistry(config):
    """Reject lead-acid-only keys for lifepo4, resolve cell_count, and fill in defaults."""
    chemistry = config[CONF_CHEMISTRY]

    if cell_count := config.pop(CONF_CELL_COUNT, None):
        cell_voltage = CELL_FULL_CHARGE_VOLTAGE.get(chemistry)
        if cell_voltage is None:
            raise cv.Invalid(
                f"'cell_count' cannot be used with chemistry '{chemistry}'; "
                f"specify 'max_charge_voltage' directly instead"
            )
        config[CONF_MAX_CHARGE_VOLTAGE] = cell_count * cell_voltage

    if chemistry == CHEMISTRY_LIFEPO4:
        for key in LEAD_ACID_KEYS:
            if key in config:
                raise cv.Invalid(
                    f"'{key}' is only valid when 'chemistry' is not '{CHEMISTRY_LIFEPO4}'"
                )
        return config

    preset = LEAD_ACID_PRESETS.get(chemistry)
    if preset is None:
        # "custom": there's no preset to silently complete a partial pair from, so
        # peukert_exponent and capacity_rate must be given together or not at all.
        has_peukert = CONF_PEUKERT_EXPONENT in config
        has_rate = CONF_CAPACITY_RATE in config
        if has_peukert != has_rate:
            raise cv.Invalid(
                "'peukert_exponent' and 'capacity_rate' must be specified together"
            )

    defaults = preset or _NO_OP_DEFAULTS
    for key in LEAD_ACID_KEYS:
        config.setdefault(key, defaults[key])
    return config


capacity_ah = cv.All(
    cv.float_with_unit("capacity", "(ah|AH|Ah|aH)?"),
    cv.float_range(min=0, min_included=False),
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BatteryGauge),
            cv.Required(CONF_VOLTAGE_SOURCE): cv.use_id(sensor.Sensor),
            cv.Required(CONF_CURRENT_SOURCE): cv.use_id(sensor.Sensor),
            cv.Required(CONF_CAPACITY): capacity_ah,
            cv.Optional(CONF_EFFICIENCY, default=0.98): cv.percentage,
            cv.Optional(CONF_MAX_CHARGE_VOLTAGE): cv.voltage,
            cv.Optional(CONF_CELL_COUNT): cv.positive_not_null_int,
            cv.Optional(CONF_INITIAL_STATE): cv.percentage,
            cv.Required(CONF_CHEMISTRY): cv.one_of(*CHEMISTRIES, lower=True),
            cv.Optional(CONF_ACCEPTANCE_KNEE): cv.percentage,
            cv.Optional(CONF_PEUKERT_EXPONENT): cv.float_range(min=1.0),
            cv.Optional(CONF_CAPACITY_RATE): cv.positive_time_period,
            cv.Optional(CONF_TAIL_CURRENT): cv.percentage,
            cv.Optional(CONF_FULL_CHARGE_DWELL): cv.positive_time_period,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.has_exactly_one_key(CONF_MAX_CHARGE_VOLTAGE, CONF_CELL_COUNT),
    _validate_chemistry,
)


async def to_code(config):
    voltage_source = await cg.get_variable(config[CONF_VOLTAGE_SOURCE])
    current_source = await cg.get_variable(config[CONF_CURRENT_SOURCE])
    capacity = config[CONF_CAPACITY]
    efficiency = config[CONF_EFFICIENCY]

    chemistry_id = ID(
        f"{config[CONF_ID]}_chemistry", is_declaration=True, type=LithiumChemistry
    )
    if config[CONF_CHEMISTRY] == CHEMISTRY_LIFEPO4:
        chemistry = cg.new_Pvariable(chemistry_id)
    else:
        chemistry_id.type = LeadAcidChemistry
        capacity_rate: cv.TimePeriod = config[CONF_CAPACITY_RATE]
        rated_current = capacity / capacity_rate.total_hours
        chemistry = cg.new_Pvariable(
            chemistry_id,
            config[CONF_ACCEPTANCE_KNEE],
            config[CONF_PEUKERT_EXPONENT],
            rated_current,
            config[CONF_TAIL_CURRENT],
            config[CONF_FULL_CHARGE_DWELL].total_milliseconds,
        )

    var = cg.new_Pvariable(
        config[CONF_ID],
        voltage_source,
        current_source,
        capacity,
        efficiency,
        config[CONF_MAX_CHARGE_VOLTAGE],
        chemistry,
    )
    await cg.register_component(var, config)

    # Stable, ID-derived key for the persisted charge preference: NVS keys by size+key, so
    # this must stay derived from the user's id, not an incrementing counter.
    cg.add(var.set_preference_key(fnv1_hash_object_id(config[CONF_ID].id)))
    if initial_state := config.get(CONF_INITIAL_STATE):
        cg.add(var.set_initial_state(initial_state))
