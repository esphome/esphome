from dataclasses import dataclass, field
import pathlib

import yaml

import esphome.codegen as cg
from esphome.components.canbus import CONF_CANBUS_ID, CanbusComponent
from esphome.components.const import CONF_DEVICE_TYPE
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NAME, CONF_OPTIONS, CONF_PLATFORM
from esphome.core import CORE, ID
from esphome.cpp_types import Component
from esphome.types import ConfigType

AUTO_LOAD = ["number", "select", "sensor", "text_sensor"]
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

# Platforms that read a datapoint: one entity per datapoint per hub, or the first would never be polled
READ_DOMAINS = ("sensor", "text_sensor")
# A write entity takes its state from the read entity on the same datapoint, so the kinds have to match
LINKED_DOMAINS = {"number": "sensor", "select": "text_sensor"}

toptronic = cg.esphome_ns.namespace("toptronic")
TopTronicComponent = toptronic.class_("TopTronic", cg.Component)

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
    "GW": 1152,  # Modbus/KNX gateway
}
# Other names accepted for a device type
DEVICE_TYPE_ALIASES = {"BD": "BM"}


def validate_device_type(value: object) -> str:
    value = cv.string(value).upper()
    return cv.enum(DEVICE_TYPES)(DEVICE_TYPE_ALIASES.get(value, value))


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
        cv.Required(CONF_DEVICE_TYPE): validate_device_type,
        # Device types are 64 apart, so a larger address would alias the next type
        cv.Required(CONF_DEVICE_ADDR): cv.int_range(min=0, max=63),
    }
).extend(cv.COMPONENT_SCHEMA)


def _final_validate(config: ConfigType) -> None:
    """Generate and validate this hub's preset entities at validation time."""
    _register_preset_entities(config)
    _reject_duplicate_datapoints(config)
    _reject_mismatched_links(config)


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


def _hub_entities(hub: str, domain: str) -> list[ConfigType]:
    """Config entries of one domain that belong to this hub."""
    from esphome import final_validate as fv

    return [
        entity
        for entity in fv.full_config.get().get(domain, [])
        if entity.get(CONF_PLATFORM) == DOMAIN and str(entity[CONF_TOPTRONIC_ID]) == hub
    ]


def _datapoint_key(entity: ConfigType) -> tuple[int, int, int]:
    return (
        entity[CONF_FUNCTION_GROUP],
        entity[CONF_FUNCTION_NUMBER],
        entity[CONF_DATAPOINT],
    )


def _hub_datapoints(
    hub: str, domains: tuple[str, ...]
) -> list[tuple[tuple[int, int, int], str, str]]:
    """Key, domain and description of this hub's preset and hand-written entries."""
    entries = [
        (_datapoint_key(entity), platform_name, f"preset {platform_name}")
        for platform_name, entity in _get_data().generated_entities.get(hub, [])
        if platform_name in domains
    ]
    for domain in domains:
        entries.extend(
            (_datapoint_key(entity), domain, f"{domain} '{entity.get(CONF_NAME)}'")
            for entity in _hub_entities(hub, domain)
        )
    return entries


def _reject_duplicate_datapoints(config: ConfigType) -> None:
    """Reject two read entities on one datapoint of this hub.

    A write entity (number, select, switch) may share the datapoint with a read
    entity, it never lands in the hub's sensor map. Two read entities would
    share one map slot, so the first would never be polled and would stay
    unknown for good.
    """
    seen: dict[tuple[int, int, int], str] = {}
    for key, _, source in _hub_datapoints(str(config[CONF_ID]), READ_DOMAINS):
        if (first := seen.get(key)) is not None:
            raise cv.Invalid(
                f"Datapoint {key[2]} of function {key[0]}/{key[1]} is already read by "
                f"{first}; {source} would never receive a value"
            )
        seen[key] = source


def _reject_mismatched_links(config: ConfigType) -> None:
    """Reject a write entity that cannot take its state from the read entity on the same datapoint.

    The hub casts the input to the read entity's class when it links them, so a
    number needs a numeric sensor and a select needs a text sensor on its key.
    """
    hub = str(config[CONF_ID])
    reads = {key: domain for key, domain, _ in _hub_datapoints(hub, READ_DOMAINS)}
    for key, domain, source in _hub_datapoints(hub, tuple(LINKED_DOMAINS)):
        read_domain = LINKED_DOMAINS[domain]
        if (found := reads.get(key)) is not None and found != read_domain:
            raise cv.Invalid(
                f"{source} writes datapoint {key[2]} of function {key[0]}/{key[1]}, "
                f"which is read by a {found}; a {domain} can only take its state from a {read_domain}"
            )


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
    await _generate_entities(var, config)
