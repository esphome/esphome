from dataclasses import dataclass, field
import pathlib

import yaml

import esphome.codegen as cg
from esphome.components.canbus import CONF_CANBUS_ID, CanbusComponent
from esphome.components.const import CONF_DEVICE_TYPE
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_OPTIONS
from esphome.core import CORE, ID
from esphome.cpp_types import Component
from esphome.types import ConfigType

AUTO_LOAD = ["sensor"]
CODEOWNERS = ["@nliaudat"]
DEPENDENCIES = ["canbus"]
DOMAIN = "toptronic"
MULTI_CONF = True

CONF_TOPTRONIC_ID = "toptronic_id"
CONF_DEVICE_ADDR = "device_addr"
CONF_FUNCTION_GROUP = "function_group"
CONF_FUNCTION_NUMBER = "function_number"
CONF_DATAPOINT = "datapoint"
CONF_DECIMAL = "decimal"
CONF_VALUES = "values"
CONF_MAX_PENDING_MESSAGES = "max_pending_messages"
CONF_MAX_FRAMES_PER_MESSAGE = "max_frames_per_message"
CONF_MAX_PENDING_AGE = "max_pending_age"
CONF_CLEANUP_INTERVAL = "cleanup_interval"

toptronic = cg.esphome_ns.namespace("toptronic")
TopTronicComponent = toptronic.class_("TopTronic", cg.Component)
TopTronicBase = toptronic.class_("TopTronicBase", cg.PollingComponent)

TT_TYPE = toptronic.enum("TypeName")
_TT_TYPES = ("U8", "U16", "U32", "S8", "S16", "S32", "S64")
TT_TYPE_OPTIONS = {name: getattr(TT_TYPE, name) for name in _TT_TYPES}

DEVICE_TYPES = {
    "WEZ": 0,  # heat generator
    "SOL": 64,  # solar module
    "PS": 128,  # buffer storage tank
    "FW": 192,  # district heating
    "HK": 256,  # heating circuit
    "MWA": 384,  # energy meter module
    "GLT": 448,  # building management system
    "HV": 512,  # HomeVent ventilation
    "BM": 1024,  # control module (display)
    "BD": 1024,  # alias of BM
    "GW": 1153,  # Modbus/KNX gateway
}

PRESETS_DIR = pathlib.Path(__file__).parent / "presets"
_PRESETS_KEY = "toptronic_used_ids"


@dataclass
class TopTronicData:
    """Per-run state shared between final validation and code generation."""

    # Validated preset entity configs per hub ID (built in FINAL_VALIDATE_SCHEMA
    # so their Component IDs are recorded early enough to count into
    # ESPHOME_COMPONENT_COUNT).
    generated_entities: dict[str, list[tuple[str, ConfigType]]] = field(
        default_factory=dict
    )


def _get_data() -> TopTronicData:
    """Return this component's per-run state from CORE.data."""
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = TopTronicData()
    return CORE.data[DOMAIN]


def _shared_used_ids():
    """Build-wide set of used IDs, shared across all hub instances."""
    if _PRESETS_KEY not in CORE.data:
        # Seed with every ID name in the config tree (post ID pass, reached via
        # the final-validation context) so generated preset IDs cannot collide
        # with IDs the user already declared.
        from esphome import final_validate as fv
        from esphome.config import iter_ids

        result = fv.full_config.get()
        used = {id.id for id, _ in iter_ids(result) if id.id is not None}
        CORE.data[_PRESETS_KEY] = used
    return CORE.data[_PRESETS_KEY]


def _validate_options_values_lengths(config: ConfigType) -> ConfigType:
    if len(config[CONF_OPTIONS]) != len(config[CONF_VALUES]):
        raise cv.Invalid(
            f"'options' length ({len(config[CONF_OPTIONS])}) must match "
            f"'values' length ({len(config[CONF_VALUES])})"
        )
    return config


CONFIG_SCHEMA_BASE = cv.Schema(
    {
        cv.Required(CONF_FUNCTION_GROUP): cv.uint8_t,
        cv.Required(CONF_FUNCTION_NUMBER): cv.uint8_t,
        cv.Required(CONF_DATAPOINT): cv.uint16_t,
    }
)


def config_schema_polling(update_interval: str = "30s"):
    return CONFIG_SCHEMA_BASE.extend(cv.polling_component_schema(update_interval))


# Presets are optional: a device type without a preset directory can still be
# used with hand-written entities.
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(TopTronicComponent),
        cv.GenerateID(CONF_CANBUS_ID): cv.use_id(CanbusComponent),
        cv.Required(CONF_DEVICE_TYPE): cv.enum(DEVICE_TYPES, upper=True),
        # Device types are 64 apart, so a larger address would alias the next type
        cv.Required(CONF_DEVICE_ADDR): cv.int_range(min=0, max=63),
        # Each pending message is a fixed reassembly slot of max_frames_per_message * 7 bytes
        cv.Optional(CONF_MAX_PENDING_MESSAGES, default=2): cv.int_range(min=1, max=16),
        # The 5-bit frame count field allows at most 31 frames
        cv.Optional(CONF_MAX_FRAMES_PER_MESSAGE, default=8): cv.int_range(
            min=3, max=31
        ),
        cv.Optional(
            CONF_MAX_PENDING_AGE, default="5000ms"
        ): cv.positive_time_period_milliseconds,
        # Stale slots are reclaimed when the next message needs one, so no periodic sweep runs;
        # the key is accepted and has no effect
        cv.Optional(CONF_CLEANUP_INTERVAL): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)

# Reassembly storage is sized for the largest hub
_request_slot = cg.slot_counter("TOPTRONIC_SLOTS")
_request_frame = cg.slot_counter("TOPTRONIC_MAX_FRAMES")


def _final_validate(config: ConfigType) -> None:
    """Generate and validate this hub's preset entities at validation time."""
    _register_preset_entities(config)


FINAL_VALIDATE_SCHEMA = _final_validate


def _load_entities(device_type: str):
    entities = []
    for kind in ("sensors", "inputs", "buttons"):
        path = PRESETS_DIR / device_type / f"{kind}_en.yaml"
        if not path.exists():
            continue
        with pathlib.Path(path).open(encoding="utf-8") as f:
            data = yaml.safe_load(f) or {}
        for platform_name, entries in data.items():
            entities.extend((platform_name, dict(entry)) for entry in entries or [])
    return entities


def _resolve_ids(obj, used=None):
    """Resolve auto-generated IDs inside a validated entity config.

    Entities synthesized from presets bypass config.py's global ID pass, so
    nested IDs (e.g. filter IDs) keep id=None. Resolve them in final validation
    (after the ID pass) to avoid duplicate empty-ID registrations during code
    generation, and record Component-derived declarations in
    CORE.component_ids exactly as config.py's ID pass would.
    """
    if used is None:
        used = set()
    if isinstance(obj, ID):
        if obj.id is None:
            obj.resolve(used)
        elif obj.is_declaration and obj.id in used:
            raise cv.Invalid(f"ID '{obj.id}' is already used by another entity")
        used.add(obj.id)
        if (
            obj.is_declaration
            and isinstance(obj.type, cg.MockObjClass)
            and obj.type.inherits_from(Component)
        ):
            CORE.component_ids.add(obj.id)
    elif isinstance(obj, dict):
        for value in obj.values():
            _resolve_ids(value, used)
    elif isinstance(obj, (list, tuple)):
        for value in obj:
            _resolve_ids(value, used)
    return used


def _register_preset_entities(config: ConfigType) -> None:
    """Validate the preset entities and register them at validation time.

    The preset entity configs bypass config.py's global ID pass (they do not
    live in the config tree), so they are generated and validated here in
    FINAL_VALIDATE_SCHEMA. Component-derived IDs are recorded in
    CORE.component_ids now, because ESPHOME_COMPONENT_COUNT is emitted from the
    core component's to_code() (CoroPriority.CORE), which runs BEFORE any
    component to_code(): IDs registered later never make it into the define and
    their App::components_ push_back (StaticVector) is silently dropped, so
    those sensors would never poll.

    The validated configs are stashed in CORE.data for to_code to generate.
    """
    from . import number, select, sensor, text_sensor

    platforms = {
        "sensor": sensor.CONFIG_SCHEMA,
        "text_sensor": text_sensor.CONFIG_SCHEMA,
        "number": number.CONFIG_SCHEMA,
        "select": select.CONFIG_SCHEMA,
    }
    used_ids = _shared_used_ids()
    generated = []
    for platform_name, entity_conf in _load_entities(config[CONF_DEVICE_TYPE]):
        if platform_name not in platforms:
            raise cv.Invalid(
                f"Unsupported platform '{platform_name}' in toptronic preset"
            )
        entity_conf.pop("platform", None)
        entity_conf.pop(CONF_DEVICE_TYPE, None)
        entity_conf.pop(CONF_DEVICE_ADDR, None)
        hub_ref = config[CONF_ID].copy()
        hub_ref.is_declaration = False
        entity_conf[CONF_TOPTRONIC_ID] = hub_ref
        validated = platforms[platform_name](entity_conf)
        _resolve_ids(validated, used_ids)
        generated.append((platform_name, validated))

    _get_data().generated_entities[str(config[CONF_ID])] = generated


async def _generate_entities(hub, config: ConfigType) -> None:
    """Generate the preset entities validated during final validation."""
    from . import number, select, sensor, text_sensor

    codegens = {
        "sensor": sensor.to_code,
        "text_sensor": text_sensor.to_code,
        "number": number.to_code,
        "select": select.to_code,
    }
    saved = _get_data().generated_entities.get(str(config[CONF_ID]), [])
    for platform_name, validated in saved:
        await codegens[platform_name](validated)


async def to_code(config: ConfigType) -> None:
    cbus = await cg.get_variable(config[CONF_CANBUS_ID])
    device_id = DEVICE_TYPES[config[CONF_DEVICE_TYPE]] | config[CONF_DEVICE_ADDR]
    var = cg.new_Pvariable(config[CONF_ID], cbus, device_id)
    await cg.register_component(var, config)
    max_pending = config[CONF_MAX_PENDING_MESSAGES]
    max_frames = config[CONF_MAX_FRAMES_PER_MESSAGE]
    for _ in range(max_pending):
        _request_slot(str(var))
    for _ in range(max_frames):
        _request_frame(str(var))
    cg.add(var.set_max_pending_messages(max_pending))
    cg.add(var.set_max_frames_per_message(max_frames))
    cg.add(var.set_max_pending_age_ms(config[CONF_MAX_PENDING_AGE]))
    await _generate_entities(var, config)
