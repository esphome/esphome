from dataclasses import dataclass, field
import pathlib
from typing import Any

import yaml

import esphome.codegen as cg
from esphome.components.canbus import CONF_CANBUS_ID, CanbusComponent
from esphome.components.const import CONF_DEVICE_TYPE
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NAME, CONF_OPTIONS, CONF_PLATFORM, CONF_TYPE
from esphome.core import CORE, ID
from esphome.cpp_types import Component
from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor
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
# Platforms that write a datapoint: the hub keys inputs by datapoint, so only one may own it
WRITE_DOMAINS = ("number", "select", "button", "switch")
# A write entity takes its state from the read entity on the same datapoint, so the kinds have to match
LINKED_DOMAINS = {"number": "sensor", "select": "text_sensor"}

# Bounds of each wire type, used to reject values the encoder would clamp
TYPE_RANGES = {
    "U8": (0, 0xFF),
    "U16": (0, 0xFFFF),
    "U32": (0, 0xFFFFFFFF),
    "S8": (-(1 << 7), (1 << 7) - 1),
    "S16": (-(1 << 15), (1 << 15) - 1),
    "S32": (-(1 << 31), (1 << 31) - 1),
    "S64": (-(1 << 63), (1 << 63) - 1),
}

toptronic = cg.esphome_ns.namespace("toptronic")
TopTronicComponent = toptronic.class_("TopTronic", cg.Component)
Option = toptronic.struct("Option")

# Entity registries are fixed at code generation, one list per hub
_request_sensor = cg.slot_counter("TOPTRONIC_SENSOR_COUNT")
_request_input = cg.slot_counter("TOPTRONIC_INPUT_COUNT")

TT_TYPE = toptronic.enum("TypeName", is_class=True)
_TT_TYPES = ("U8", "U16", "U32", "S8", "S16", "S32", "S64")
TT_TYPE_OPTIONS = {name: getattr(TT_TYPE, f"TYPE_NAME_{name}") for name in _TT_TYPES}

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


@schema_extractor("enum")
def validate_device_type(value: object) -> str:
    if value is SCHEMA_EXTRACT:
        return {**DEVICE_TYPES, "BD": DEVICE_TYPES["BM"]}
    value = cv.string(value).upper()
    return cv.enum(DEVICE_TYPES)(DEVICE_TYPE_ALIASES.get(value, value))


PRESETS_DIR: pathlib.Path = pathlib.Path(__file__).parent / "presets"


@dataclass
class TopTronicData:
    """Per-run state shared between final validation and code generation."""

    # Validated preset entity configs per hub ID (built in FINAL_VALIDATE_SCHEMA
    # so their Component IDs are recorded early enough to count into
    # ESPHOME_COMPONENT_COUNT).
    generated_entities: dict[str, list[tuple[str, ConfigType]]] = field(
        default_factory=dict
    )
    # Build-wide set of used IDs, seeded once from the whole config tree
    used_ids: set[str] | None = None


def _get_data() -> TopTronicData:
    """Return this component's per-run state from CORE.data."""
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = TopTronicData()
    return CORE.data[DOMAIN]


def _shared_used_ids() -> set[str]:
    """Build-wide set of used IDs, shared across all hub instances."""
    data = _get_data()
    if data.used_ids is None:
        # Seed with every ID name in the config tree (post ID pass, reached via
        # the final-validation context) so generated preset IDs cannot collide
        # with IDs the user already declared.
        from esphome import final_validate as fv
        from esphome.config import iter_ids

        result = fv.full_config.get()
        data.used_ids = {id.id for id, _ in iter_ids(result) if id.id is not None}
    return data.used_ids


def _option_array(entity_id: ID, values: list[int], labels: list[str]) -> cg.MockObj:
    """Build one flash table of value/label pairs for an entity."""
    entries = [
        cg.StructInitializer(Option, ("value", value), ("text", label))
        for value, label in zip(values, labels, strict=True)
    ]
    return cg.static_const_array(
        ID(f"{entity_id}_options", is_declaration=True, type=Option),
        cg.ArrayInitializer(*entries),
    )


def _value_array(entity_id: ID, values: list[int]) -> cg.MockObj:
    """Build one flash table of wire values for a select."""
    return cg.static_const_array(
        ID(f"{entity_id}_values", is_declaration=True, type=cg.uint8),
        cg.ArrayInitializer(*values),
    )


def _validate_options_values_lengths(config: ConfigType) -> ConfigType:
    if len(config[CONF_OPTIONS]) != len(config[CONF_VALUES]):
        raise cv.Invalid(
            f"'options' length ({len(config[CONF_OPTIONS])}) must match "
            f"'values' length ({len(config[CONF_VALUES])})"
        )
    if len(set(config[CONF_VALUES])) != len(config[CONF_VALUES]):
        raise cv.Invalid("'values' must be unique")
    return config


CONFIG_SCHEMA_BASE = cv.Schema(
    {
        cv.Required(CONF_FUNCTION_GROUP): cv.uint8_t,
        cv.Required(CONF_FUNCTION_NUMBER): cv.uint8_t,
        cv.Required(CONF_DATAPOINT): cv.uint16_t,
    }
)


def config_schema_polling(update_interval: str = "30s") -> cv.Schema:
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


def _load_entities(device_type: str) -> list[tuple[str, ConfigType]]:
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


def _resolve_ids(obj: Any, used: set[str] | None = None) -> set[str]:
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
) -> list[tuple[tuple[int, int, int], str, str, ConfigType]]:
    """Key, domain, description and config of this hub's preset and hand-written entries."""
    entries = [
        (
            _datapoint_key(entity),
            platform_name,
            f"preset {platform_name}",
            entity,
        )
        for platform_name, entity in _get_data().generated_entities.get(hub, [])
        if platform_name in domains
    ]
    for domain in domains:
        entries.extend(
            (
                _datapoint_key(entity),
                domain,
                f"{domain} '{entity.get(CONF_NAME)}'",
                entity,
            )
            for entity in _hub_entities(hub, domain)
        )
    return entries


def _reject_duplicate_datapoints(config: ConfigType) -> None:
    """Reject two read or two write entities on one datapoint of this hub.

    A write entity may share the datapoint with a read entity: it never lands in
    the hub's sensor map and takes its state from that read entity. Two entities
    of the same direction would overwrite each other in the hub's registry, so
    the first would never be polled or never send a write.
    """
    _reject_duplicates(config, READ_DOMAINS, "read", "would never receive a value")
    _reject_duplicates(config, WRITE_DOMAINS, "written", "would never send a value")


def _reject_duplicates(
    config: ConfigType, domains: tuple[str, ...], verb: str, reason: str
) -> None:
    seen: dict[tuple[int, int, int], str] = {}
    for key, _, source, _ in _hub_datapoints(str(config[CONF_ID]), domains):
        if (first := seen.get(key)) is not None:
            raise cv.Invalid(
                f"Datapoint {key[2]} of function {key[0]}/{key[1]} is already {verb} by "
                f"{first}; {source} {reason}"
            )
        seen[key] = source


def _reject_mismatched_links(config: ConfigType) -> None:
    """Reject a write entity that cannot take its state from the read entity on the same datapoint.

    The hub casts the input to the read entity's class when it links them, so a
    number needs a numeric sensor and a select needs a text sensor on its key.
    The two entities must also agree on the wire type and the option mapping, or
    the write entity would show or send a different value from the one read.
    """
    hub = str(config[CONF_ID])
    reads = {
        key: (domain, entity)
        for key, domain, _, entity in _hub_datapoints(hub, READ_DOMAINS)
    }
    for key, domain, source, write in _hub_datapoints(hub, tuple(LINKED_DOMAINS)):
        read_domain = LINKED_DOMAINS[domain]
        found = reads.get(key)
        if found is None:
            continue
        found_domain, read = found
        if found_domain != read_domain:
            raise cv.Invalid(
                f"{source} writes datapoint {key[2]} of function {key[0]}/{key[1]}, "
                f"which is read by a {found_domain}; a {domain} can only take its state from a {read_domain}"
            )
        write_type = write.get(CONF_TYPE)
        read_type = read.get(CONF_TYPE)
        if (
            domain == "number"
            and write_type is not None
            and read_type is not None
            and write_type != read_type
        ):
            raise cv.Invalid(
                f"{source} writes datapoint {key[2]} of function {key[0]}/{key[1]} as "
                f"{write_type}, but the sensor reads it as {read_type}; "
                f"the wire types must match"
            )
        if domain == "select" and (
            write.get(CONF_OPTIONS) != read.get(CONF_OPTIONS)
            or write.get(CONF_VALUES) != read.get(CONF_VALUES)
        ):
            raise cv.Invalid(
                f"{source} writes datapoint {key[2]} of function {key[0]}/{key[1]}, "
                f"but its 'options'/'values' mapping differs from the text sensor's"
            )


async def _generate_entities(hub: cg.MockObj, config: ConfigType) -> None:
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
